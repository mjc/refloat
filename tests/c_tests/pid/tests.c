#include "c_tests/test_support.h"

static bool test_pid_update_scales_and_integral_limit(void) {
    PID pid = {0};
    MotorData motor = {0};
    IMU imu = {0};
    RefloatConfig config = {0};

    pid_init(&pid);
    EXPECT_FLOAT_NEAR(pid.p, 0.0f);
    EXPECT_FLOAT_NEAR(pid.i, 0.0f);
    EXPECT_FLOAT_NEAR(pid.rate_p, 0.0f);
    EXPECT_FLOAT_NEAR(pid.p_fwd_scale.value, 1.0f);
    EXPECT_FLOAT_NEAR(pid.p_bwd_scale.value, 1.0f);

    pid_configure(&pid, 1000.0f);
    config.kp = 1.0f;
    config.ki = 10.0f;
    config.ki_limit = 0.01f;
    config.kp2 = 1.0f;
    config.kp_brake = 0.5f;
    config.kp2_brake = 0.25f;
    imu.balance_pitch = 0.1f;
    imu.pitch_rate = 0.2f;

    motor.erpm = -1000.0f;
    pid_update(&pid, 1.0f, &motor, &imu, &config, 0.01f);
    EXPECT_TRUE(isfinite(pid.p));
    EXPECT_TRUE(isfinite(pid.i));
    EXPECT_TRUE(isfinite(pid.rate_p));
    EXPECT_TRUE(fabsf(pid.i) <= config.ki_limit * TORQUE_CONSTANT_COMPAT);
    EXPECT_TRUE(pid.p_fwd_scale.value < 1.0f);
    EXPECT_TRUE(pid.rate_p_fwd_scale.value < 1.0f);

    motor.erpm = 1000.0f;
    imu.balance_pitch = 2.0f;
    imu.pitch_rate = -0.2f;
    pid_update(&pid, 0.0f, &motor, &imu, &config, 0.01f);
    EXPECT_TRUE(isfinite(pid.p));
    EXPECT_TRUE(isfinite(pid.rate_p));
    EXPECT_TRUE(pid.p_bwd_scale.value < 1.0f);
    EXPECT_TRUE(pid.rate_p_bwd_scale.value < 1.0f);

    pid_update(&pid, 0.0f, &motor, &imu, &config, 0.0f);
    EXPECT_FLOAT_NEAR(pid.p, 0.0f);
    EXPECT_FLOAT_NEAR(pid.rate_p, 0.0f);
    return true;
}

static bool test_pid_valid_inputs_preserve_finite_state(void) {
    uint32_t seed = 0x51d1c0deu;

    for (size_t case_index = 0; case_index < 256; ++case_index) {
        PID pid = {0};
        MotorData motor = {0};
        IMU imu = {0};
        RefloatConfig config = {0};

        pid_init(&pid);
        pid_configure(&pid, 1000.0f);
        config.kp = test_random_range(&seed, 0.0f, 2.0f);
        config.ki = test_random_range(&seed, 0.0f, 8.0f);
        config.ki_limit = test_random_range(&seed, 0.01f, 0.5f);
        config.kp2 = test_random_range(&seed, 0.0f, 2.0f);
        config.kp_brake = test_random_range(&seed, 0.1f, 1.5f);
        config.kp2_brake = test_random_range(&seed, 0.1f, 1.5f);
        motor.erpm = test_random_range(&seed, -2500.0f, 2500.0f);
        imu.balance_pitch = test_random_range(&seed, -2.0f, 2.0f);
        imu.pitch_rate = test_random_range(&seed, -5.0f, 5.0f);
        float setpoint = test_random_range(&seed, -2.0f, 2.0f);
        float dt = test_random_range(&seed, 0.0005f, 0.02f);

        pid_update(&pid, setpoint, &motor, &imu, &config, dt);

        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, isfinite(pid.p));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, isfinite(pid.i));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, isfinite(pid.rate_p));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, isfinite(pid.p_fwd_scale.value));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, isfinite(pid.p_bwd_scale.value));
        TEST_EXPECT_TRUE_WITH_CONTEXT(
            seed, case_index, fabsf(pid.i) <= config.ki_limit * TORQUE_CONSTANT_COMPAT + 1e-5f
        );
    }

    return true;
}

static bool test_pid_bounds_control_gains(void) {
    // ConfigParams/XML and raw COMM_SET_CUSTOM_CONFIG packets can bypass the
    // VESC Tool widgets' XML minima and preserve these signed float16 values.
    PID pid = {0};
    MotorData motor = {.erpm = -1000.0f};
    IMU imu = {.balance_pitch = 0.0f, .pitch_rate = -1.0f};
    RefloatConfig config = {
        .kp = -1.0f,
        .ki = -1.0f,
        .kp2 = -1.0f,
        .kp_brake = -1.0f,
        .kp2_brake = -1.0f,
    };

    pid_init(&pid);
    pid.p_fwd_scale.alpha = 1.0f;
    pid.rate_p_fwd_scale.alpha = 1.0f;
    pid_update(&pid, 1.0f, &motor, &imu, &config, 0.01f);

    EXPECT_FLOAT_NEAR(pid.p, 0.0f);
    EXPECT_FLOAT_NEAR(pid.i, 0.0f);
    EXPECT_FLOAT_NEAR(pid.rate_p, 0.0f);
    EXPECT_FLOAT_NEAR(pid.p_fwd_scale.value, 0.0f);
    EXPECT_FLOAT_NEAR(pid.rate_p_fwd_scale.value, 0.0f);

    config = (RefloatConfig){
        .kp = 100.0f,
        .ki = 100.0f,
        .kp2 = 100.0f,
        .kp_brake = 100.0f,
        .kp2_brake = 100.0f,
        .ki_limit = 1000.0f,
    };
    pid_init(&pid);
    pid.p_fwd_scale.alpha = 1.0f;
    pid.rate_p_fwd_scale.alpha = 1.0f;
    motor.erpm = 0.0f;
    imu.pitch_rate = -1.0f;
    pid_update(&pid, 1.0f, &motor, &imu, &config, 0.01f);
    EXPECT_FLOAT_NEAR(pid.p, 40.0f * TORQUE_CONSTANT_COMPAT);
    EXPECT_FLOAT_NEAR(pid.i, 3.6f * TORQUE_CONSTANT_COMPAT);
    EXPECT_FLOAT_NEAR(pid.rate_p, 3.0f * TORQUE_CONSTANT_COMPAT);

    config.kp = config.ki = config.kp2 = 0.0f;
    motor.erpm = -1000.0f;
    pid_update(&pid, 1.0f, &motor, &imu, &config, 0.01f);
    EXPECT_FLOAT_NEAR(pid.p_fwd_scale.value, 3.0f);
    EXPECT_FLOAT_NEAR(pid.rate_p_fwd_scale.value, 3.0f);

    pid.i = 600.0f * TORQUE_CONSTANT_COMPAT;
    imu.balance_pitch = 1.0f;
    pid_update(&pid, 1.0f, &motor, &imu, &config, 0.01f);
    EXPECT_FLOAT_NEAR(pid.i, 500.0f * TORQUE_CONSTANT_COMPAT);

    return true;
}
