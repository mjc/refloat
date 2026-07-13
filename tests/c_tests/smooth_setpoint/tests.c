static bool test_smooth_setpoint_negative_time_constants(void) {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);

    smooth_setpoint_configure(&st, -0.1f, -0.2f, -0.3f, -0.4f, 10.0f, 20.0f, 30.0f, 40.0f, 100.0f);

    EXPECT_TRUE(isfinite(st.alpha));
    EXPECT_TRUE(isfinite(st.on_speed_alpha));
    EXPECT_TRUE(isfinite(st.off_speed_alpha));
    EXPECT_TRUE(isfinite(st.winddown_alpha));

    EXPECT_TRUE(st.alpha >= 0.0f);
    EXPECT_TRUE(st.on_speed_alpha >= 0.0f);
    EXPECT_TRUE(st.off_speed_alpha >= 0.0f);
    EXPECT_TRUE(st.winddown_alpha >= 0.0f);

    return true;
}

static bool test_smooth_setpoint_rejects_negative_speed_limits(void) {
    // ConfigParams/XML and raw COMM_SET_CUSTOM_CONFIG packets can bypass the
    // VESC Tool widgets' XML minima and preserve these signed float16 values.
    SmoothSetpoint st;
    smooth_setpoint_init(&st);

    smooth_setpoint_configure(&st, 0.1f, 0.1f, 0.1f, 0.1f, -10.0f, -20.0f, -30.0f, -40.0f, 100.0f);

    EXPECT_FLOAT_NEAR(st.on_speed_up, 0.0f);
    EXPECT_FLOAT_NEAR(st.off_speed_up, 0.0f);
    EXPECT_FLOAT_NEAR(st.on_speed_down, 0.0f);
    EXPECT_FLOAT_NEAR(st.off_speed_down, 0.0f);

    return true;
}

static bool test_smooth_setpoint_winddown_restart(void) {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);
    smooth_setpoint_configure(&st, 0.1f, 0.1f, 0.1f, 0.1f, 10.0f, 10.0f, 10.0f, 10.0f, 100.0f);
    st.alpha = 0.0f;
    st.value = 4.0f;
    st.v1 = 3.0f;
    smooth_setpoint_winddown(&st);
    EXPECT_TRUE(st.is_winddown);
    float winddown_value = st.value;
    smooth_setpoint_update(&st, 6.0f, true, 1.0f, 0.01f);
    EXPECT_FALSE(st.is_winddown);
    EXPECT_FLOAT_NEAR(st.v1, winddown_value);
    EXPECT_TRUE(isfinite(st.value));
    return true;
}

static bool test_smooth_setpoint_rejects_invalid_update_inputs(void) {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);
    smooth_setpoint_configure(&st, 0.1f, 0.1f, 0.1f, 0.1f, 10.0f, 10.0f, 10.0f, 10.0f, 100.0f);
    st.value = 2.0f;
    st.v1 = 2.0f;
    st.step = 1.0f;

    smooth_setpoint_update(&st, 5.0f, true, 1.0f, 0.0f);
    smooth_setpoint_update(&st, NAN, true, 1.0f, 0.01f);
    smooth_setpoint_update(&st, 5.0f, true, NAN, 0.01f);
    smooth_setpoint_update(&st, 5.0f, true, -1.0f, 0.01f);
    EXPECT_FLOAT_NEAR(st.value, 2.0f);
    EXPECT_FLOAT_NEAR(st.v1, 2.0f);
    EXPECT_FLOAT_NEAR(st.step, 1.0f);

    smooth_setpoint_update(&st, -1.0f, false, 1.0f, 0.01f);
    EXPECT_TRUE(st.step < 1.0f);
    return true;
}
