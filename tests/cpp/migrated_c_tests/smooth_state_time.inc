static bool test_smooth_setpoint_negative_time_constants(void) {
    SmoothSetpoint st;
    smooth_setpoint_init(&st);

    smooth_setpoint_configure(
        &st, -0.1f, -0.2f, -0.3f, -0.4f, 10.0f, 20.0f, 30.0f, 40.0f, 100.0f
    );

    CHECK(isfinite(st.alpha));
    CHECK(isfinite(st.on_speed_alpha));
    CHECK(isfinite(st.off_speed_alpha));
    CHECK(isfinite(st.winddown_alpha));

    CHECK(st.alpha >= 0.0f);
    CHECK(st.on_speed_alpha >= 0.0f);
    CHECK(st.off_speed_alpha >= 0.0f);
    CHECK(st.winddown_alpha >= 0.0f);

    return true;
}

static bool test_frequency_tracker_nonpositive_dt(void) {
    FrequencyTracker ft;
    Time time = {.now = 1000u};
    frequency_tracker_init(&ft, 100.0f, &time);

    frequency_tracker_update(&ft, 0.01f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    CHECK(isfinite(ft.frequency.value));
    CHECK(ft.frequency.value > 0.0f);

    float frequency_before = ft.frequency.value;
    frequency_tracker_update(&ft, 0.0f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    CHECK_FLOAT_NEAR(ft.frequency.value, frequency_before);
    CHECK(isfinite(ft.frequency.value));
    CHECK(ft.frequency.value > 0.0f);

    frequency_tracker_update(&ft, -0.01f);
    CHECK_FLOAT_NEAR(ft.dt, 10.0f);
    CHECK_FLOAT_NEAR(ft.frequency.value, frequency_before);
    CHECK(isfinite(ft.frequency.value));
    CHECK(ft.frequency.value > 0.0f);

    return true;
}
