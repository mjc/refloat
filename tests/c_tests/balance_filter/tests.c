#include "c_tests/test_support.h"

static BalanceFilterData level_balance_filter(void) {
    return (BalanceFilterData){
        .q0 = 1.0f,
        .acc_mag = 1.0f,
        .kp_pitch = 1.0f,
        .kp_roll = 1.0f,
        .kp_yaw = 1.0f,
    };
}

static BalanceFilterData balance_filter_random_valid_state(uint32_t *state) {
    float q0 = test_random_range(state, -1.0f, 1.0f);
    float q1 = test_random_range(state, -1.0f, 1.0f);
    float q2 = test_random_range(state, -1.0f, 1.0f);
    float q3 = test_random_range(state, -1.0f, 1.0f);
    float norm = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);

    if (norm < 0.0001f) {
        q0 = 1.0f;
        q1 = 0.0f;
        q2 = 0.0f;
        q3 = 0.0f;
        norm = 1.0f;
    }

    float inv_norm = 1.0f / norm;

    return (BalanceFilterData){
        .q0 = q0 * inv_norm,
        .q1 = q1 * inv_norm,
        .q2 = q2 * inv_norm,
        .q3 = q3 * inv_norm,
        .acc_mag = 1.0f,
        .kp_pitch = test_random_range(state, 0.25f, 2.5f),
        .kp_roll = test_random_range(state, 0.25f, 2.5f),
        .kp_yaw = test_random_range(state, 0.25f, 2.5f),
    };
}

static void balance_filter_expected_gravity(const BalanceFilterData *bf, float accel[3]) {
    accel[0] = 2.0f * (bf->q1 * bf->q3 - bf->q0 * bf->q2);
    accel[1] = 2.0f * (bf->q0 * bf->q1 + bf->q2 * bf->q3);
    accel[2] = bf->q0 * bf->q0 - bf->q1 * bf->q1 - bf->q2 * bf->q2 + bf->q3 * bf->q3;
}

static float balance_filter_norm(const BalanceFilterData *bf) {
    return sqrtf(bf->q0 * bf->q0 + bf->q1 * bf->q1 + bf->q2 * bf->q2 + bf->q3 * bf->q3);
}

static bool test_balance_filter_configure_asymmetric_kp(void) {
    BalanceFilterData bf = {0};
    RefloatConfig cfg = {
        .mahony_kp = 1.75f,
        .mahony_kp_roll = 0.5f,
    };

    balance_filter_configure(&bf, &cfg);
    EXPECT_FLOAT_NEAR(bf.kp_pitch, 1.75f);
    EXPECT_FLOAT_NEAR(bf.kp_roll, 0.5f);
    EXPECT_FLOAT_NEAR(bf.kp_yaw, 1.125f);

    return true;
}

static bool test_balance_filter_bounds_gains(void) {
    // ConfigParams/XML and COMM_SET_CUSTOM_CONFIG over BLE preserve signed float16 values.
    const RefloatConfig cases[] = {
        {.mahony_kp = -1.0f, .mahony_kp_roll = 1.0f},
        {.mahony_kp = 1.0f, .mahony_kp_roll = -1.0f},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        BalanceFilterData bf = {0};

        balance_filter_configure(&bf, &cases[i]);
        EXPECT_TRUE(bf.kp_pitch >= 0.0f);
        EXPECT_TRUE(bf.kp_roll >= 0.0f);
        EXPECT_FLOAT_NEAR(bf.kp_yaw, 0.5f);
    }

    BalanceFilterData bf = {0};
    RefloatConfig high = {.mahony_kp = 100.0f, .mahony_kp_roll = 100.0f};
    balance_filter_configure(&bf, &high);
    EXPECT_FLOAT_NEAR(bf.kp_pitch, 3.0f);
    EXPECT_FLOAT_NEAR(bf.kp_roll, 3.0f);
    EXPECT_FLOAT_NEAR(bf.kp_yaw, 3.0f);

    return true;
}

static bool test_balance_filter_valid_samples_stay_finite(void) {
    uint32_t seed = 0x7b11c0deu;

    for (size_t i = 0; i < 64; ++i) {
        BalanceFilterData bf = balance_filter_random_valid_state(&seed);
        float gyro[3] = {
            test_random_range(&seed, -1.5f, 1.5f),
            test_random_range(&seed, -1.5f, 1.5f),
            test_random_range(&seed, -1.5f, 1.5f),
        };
        float accel[3];
        float scale = test_random_range(&seed, 0.97f, 1.03f);
        float dt = test_random_range(&seed, 0.002f, 0.015f);

        balance_filter_expected_gravity(&bf, accel);
        accel[0] *= scale;
        accel[1] *= scale;
        accel[2] *= scale;

        balance_filter_update(&bf, gyro, accel, dt);

        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(bf.q0));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(bf.q1));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(bf.q2));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(bf.q3));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(balance_filter_get_roll(&bf)));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(balance_filter_get_pitch(&bf)));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, isfinite(balance_filter_get_yaw(&bf)));
        TEST_EXPECT_TRUE_WITH_CONTEXT(seed, i, fabsf(balance_filter_norm(&bf) - 1.0f) < 0.001f);
        TEST_EXPECT_TRUE_WITH_CONTEXT(
            seed, i, fabsf(balance_filter_get_pitch(&bf)) <= (M_PI / 2.0f + 0.001f)
        );
    }

    return true;
}

static bool test_balance_filter_stationary_level_stability(void) {
    BalanceFilterData bf = level_balance_filter();
    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.0f, 0.0f, 1.0f};

    for (size_t i = 0; i < 24; ++i) {
        balance_filter_update(&bf, gyro, accel, 0.01f);
    }

    EXPECT_FLOAT_NEAR(bf.q0, 1.0f);
    EXPECT_FLOAT_NEAR(bf.q1, 0.0f);
    EXPECT_FLOAT_NEAR(bf.q2, 0.0f);
    EXPECT_FLOAT_NEAR(bf.q3, 0.0f);
    EXPECT_FLOAT_NEAR(balance_filter_get_roll(&bf), 0.0f);
    EXPECT_FLOAT_NEAR(balance_filter_get_pitch(&bf), 0.0f);
    EXPECT_FLOAT_NEAR(balance_filter_get_yaw(&bf), 0.0f);

    return true;
}

static bool test_balance_filter_tiny_accel_skips_feedback(void) {
    BalanceFilterData bf = level_balance_filter();
    bf.q0 = cosf(deg2rad(12.0f) * 0.5f);
    bf.q2 = sinf(deg2rad(12.0f) * 0.5f);

    float gyro[3] = {0.0f, 0.0f, 0.0f};
    float accel[3] = {0.001f, 0.0f, 0.0f};
    float pitch_before = balance_filter_get_pitch(&bf);

    balance_filter_update(&bf, gyro, accel, 0.01f);

    EXPECT_FLOAT_NEAR(balance_filter_get_pitch(&bf), pitch_before);
    EXPECT_FLOAT_NEAR(balance_filter_get_roll(&bf), 0.0f);
    EXPECT_TRUE(isfinite(balance_filter_norm(&bf)));

    return true;
}

static bool test_balance_filter_clamps_pitch_estimate(void) {
    BalanceFilterData bf = level_balance_filter();

    bf.q0 = 1.0f;
    bf.q1 = 0.0f;
    bf.q2 = -1.0f;
    bf.q3 = 0.0f;
    EXPECT_FLOAT_NEAR(balance_filter_get_pitch(&bf), -M_PI / 2.0f);

    bf.q2 = 1.0f;
    EXPECT_FLOAT_NEAR(balance_filter_get_pitch(&bf), M_PI / 2.0f);

    return true;
}

static bool test_balance_filter_nonfinite_dt(void) {
    BalanceFilterData bf = level_balance_filter();

    float gyro[3] = {0.5f, -0.25f, 0.125f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    balance_filter_update(&bf, gyro, accel, 0.0f);

    EXPECT_TRUE(isfinite(bf.q0));
    EXPECT_TRUE(isfinite(bf.q1));
    EXPECT_TRUE(isfinite(bf.q2));
    EXPECT_TRUE(isfinite(bf.q3));
    EXPECT_TRUE(isfinite(balance_filter_get_roll(&bf)));
    EXPECT_TRUE(isfinite(balance_filter_get_pitch(&bf)));
    EXPECT_TRUE(isfinite(balance_filter_get_yaw(&bf)));

    float norm = sqrtf(bf.q0 * bf.q0 + bf.q1 * bf.q1 + bf.q2 * bf.q2 + bf.q3 * bf.q3);
    EXPECT_TRUE(fabsf(norm - 1.0f) < 0.00001f);

    return true;
}

static bool test_balance_filter_rejects_nonfinite_samples(void) {
    BalanceFilterData bf;
    balance_filter_init(&bf);
    float gyro[3] = {0.0f, NAN, 0.0f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    balance_filter_update(&bf, gyro, accel, 0.01f);
    EXPECT_FLOAT_NEAR(bf.q0, 1.0f);
    EXPECT_FLOAT_NEAR(bf.q1, 0.0f);

    gyro[1] = 0.0f;
    accel[1] = NAN;
    balance_filter_update(&bf, gyro, accel, 0.01f);
    accel[1] = 0.0f;

    float *quaternion[] = {&bf.q0, &bf.q1, &bf.q2, &bf.q3};
    for (size_t i = 0; i < sizeof(quaternion) / sizeof(quaternion[0]); ++i) {
        *quaternion[i] = NAN;
        balance_filter_update(&bf, gyro, accel, 0.01f);
        EXPECT_FLOAT_NEAR(bf.q0, 1.0f);
        EXPECT_TRUE(isfinite(bf.q1));
        EXPECT_TRUE(isfinite(bf.q2));
        EXPECT_TRUE(isfinite(bf.q3));
    }
    return true;
}
