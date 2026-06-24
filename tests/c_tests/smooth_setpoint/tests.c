static bool test_smooth_setpoint_negative_time_constants(void) {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);

    smooth_setpoint_configure(
        &st, -0.1f, -0.2f, -0.3f, -0.4f, 10.0f, 20.0f, 30.0f, 40.0f, 100.0f
    );

    EXPECT_TRUE(isfinite(st.alpha));
    EXPECT_TRUE(isfinite(st.on_speed_alpha));
    EXPECT_TRUE(isfinite(st.off_speed_alpha));
    EXPECT_TRUE(isfinite(st.winddown_alpha));

    XEXPECT_TRUE(st.alpha >= 0.0f);
    EXPECT_TRUE(st.on_speed_alpha >= 0.0f);
    EXPECT_TRUE(st.off_speed_alpha >= 0.0f);
    EXPECT_TRUE(st.winddown_alpha >= 0.0f);

    return true;
}
