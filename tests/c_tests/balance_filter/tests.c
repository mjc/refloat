static BalanceFilterData level_balance_filter(void) {
    return (BalanceFilterData) {
        .q0 = 1.0f,
        .acc_mag = 1.0f,
        .kp_pitch = 1.0f,
        .kp_roll = 1.0f,
        .kp_yaw = 1.0f,
    };
}

static bool test_balance_filter_nonfinite_dt(void) {
    BalanceFilterData bf = level_balance_filter();

    float gyro[3] = {0.5f, -0.25f, 0.125f};
    float accel[3] = {0.0f, 0.0f, 1.0f};
    balance_filter_update(&bf, gyro, accel, NAN);

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
