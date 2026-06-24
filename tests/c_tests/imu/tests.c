static bool test_imu_update_edges(void) {
    vesc_if_fake_reset();

    IMU imu = {
        .pitch = 9.0f,
        .balance_pitch = 8.0f,
        .roll = 7.0f,
        .yaw = 6.0f,
        .pitch_rate = 5.0f,
        .flywheel_pitch_offset = 4.0f,
        .flywheel_roll_offset = 3.0f,
    };
    imu_init(&imu);
    EXPECT_FLOAT_NEAR(imu.pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 0.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 0.0f);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 0.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_pitch_offset, 0.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_roll_offset, 0.0f);

    BalanceFilterData bf = {0};
    State state = {.mode = MODE_NORMAL, .darkride = false};
    vesc_if_fake_set_imu(0.0f, deg2rad(60.0f), 0.0f, 0.0f, 2.0f, 4.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 2.232051f);
    EXPECT_FLOAT_NEAR(imu.pitch, 0.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 60.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 0.0f);

    bf.q0 = cosf(deg2rad(10.0f) * 0.5f);
    bf.q2 = sinf(deg2rad(10.0f) * 0.5f);
    vesc_if_fake_set_imu(deg2rad(12.0f), deg2rad(30.0f), deg2rad(-15.0f), 0.0f, 3.0f, -2.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch, 12.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, 10.0f);
    EXPECT_FLOAT_NEAR(imu.roll, 30.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, -15.0f);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, 1.383975f);

    state.darkride = true;
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch_rate, -1.383975f);

    imu.pitch = 12.0f;
    imu.roll = -34.0f;
    imu_set_flywheel_offsets(&imu);
    EXPECT_FLOAT_NEAR(imu.flywheel_pitch_offset, 12.0f);
    EXPECT_FLOAT_NEAR(imu.flywheel_roll_offset, -34.0f);

    state.darkride = false;
    state.mode = MODE_FLYWHEEL;
    imu.flywheel_pitch_offset = 5.0f;
    imu.flywheel_roll_offset = 0.0f;

    vesc_if_fake_set_imu(deg2rad(2.0f), deg2rad(250.0f), deg2rad(15.0f), 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.pitch, 3.0f);
    EXPECT_FLOAT_NEAR(imu.balance_pitch, imu.pitch);
    EXPECT_FLOAT_NEAR(imu.roll, -110.0f);
    EXPECT_FLOAT_NEAR(imu.yaw, 15.0f);

    vesc_if_fake_set_imu(deg2rad(2.0f), deg2rad(-250.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 110.0f);

    return true;
}

static bool test_imu_flywheel_roll_wrap_boundaries(void) {
    vesc_if_fake_reset();

    IMU imu;
    imu_init(&imu);
    BalanceFilterData bf = {0};
    State state = {.mode = MODE_FLYWHEEL, .darkride = false};
    imu.flywheel_pitch_offset = 0.0f;
    imu.flywheel_roll_offset = 0.0f;

    vesc_if_fake_set_imu(0.0f, deg2rad(-200.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -200.0f);

    vesc_if_fake_set_imu(0.0f, deg2rad(-200.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 159.9f);

    vesc_if_fake_set_imu(0.0f, deg2rad(200.0f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, 200.0f);

    vesc_if_fake_set_imu(0.0f, deg2rad(200.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -159.9f);

    imu.flywheel_roll_offset = 40.0f;
    vesc_if_fake_set_imu(0.0f, deg2rad(240.1f), 0.0f, 0.0f, 0.0f, 0.0f);
    imu_update(&imu, &bf, &state);
    EXPECT_FLOAT_NEAR(imu.roll, -159.9f);

    return true;
}
