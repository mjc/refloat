static bool test_reverse_stop_update_paths(void) {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 1000000u};
    reverse_stop_reset(&rs, 5.0f);
    EXPECT_TRUE(!reverse_stop_active(&rs));

    reverse_stop_update(&rs, 6.0f, 0.0f, 0.0f, &time, false);
    EXPECT_FLOAT_NEAR(rs.start_distance, 6.0f);
    EXPECT_FLOAT_NEAR(rs.target_setpoint, 0.0f);

    timer_expire(&time, &rs.timer, 10.0f);
    reverse_stop_update(&rs, 5.95f, -300.0f, 12.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.target_setpoint, 17.0f);
    EXPECT_FLOAT_NEAR(rs.start_setpoint, 12.0f);
    EXPECT_TRUE(rs.target_distance < 0.0f);
    EXPECT_TRUE(reverse_stop_active(&rs));
    EXPECT_FLOAT_NEAR(reverse_stop_setpoint(&rs), 12.0f);
    EXPECT_EQ_U32(rs.timer, time.now);
    EXPECT_TRUE(!reverse_stop_stop(&rs, &time));

    reverse_stop_update(&rs, 5.75f, -300.0f, 12.0f, &time, true);
    EXPECT_TRUE(rs.progress.value > 0.0f);
    EXPECT_TRUE(reverse_stop_setpoint(&rs) > 0.0f);
    EXPECT_TRUE(!reverse_stop_stop(&rs, &time));

    ema_reset(&rs.progress, 1.0f);
    EXPECT_TRUE(reverse_stop_stop(&rs, &time));

    reverse_stop_update(&rs, 5.95f, -300.0f, 17.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.target_setpoint, 0.0f);
    EXPECT_FLOAT_NEAR(rs.start_setpoint, 17.0f);
    EXPECT_TRUE(rs.target_distance > 0.0f);
    EXPECT_TRUE(reverse_stop_active(&rs));

    timer_expire(&time, &rs.timer, 3.1f);
    EXPECT_TRUE(reverse_stop_stop(&rs, &time));

    return true;
}

static bool test_reverse_stop_completes_after_configured_duration(void) {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 2000000u};
    reverse_stop_reset(&rs, 10.0f);

    reverse_stop_update(&rs, 10.0f, 0.0f, 0.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.start_distance, 10.0f);

    reverse_stop_update(&rs, 9.9f, -250.0f, 16.99f, &time, true);
    EXPECT_FLOAT_NEAR(rs.target_setpoint, 17.0f);
    EXPECT_FLOAT_NEAR(rs.target_distance, 0.0f);
    EXPECT_FLOAT_NEAR(rs.progress.value, 1.0f);
    EXPECT_TRUE(reverse_stop_stop(&rs, &time));

    reverse_stop_reset(&rs, 20.0f);
    reverse_stop_update(&rs, 19.8f, -300.0f, 0.0f, &time, true);
    EXPECT_TRUE(reverse_stop_active(&rs));
    ema_reset(&rs.progress, 0.25f);
    timer_refresh(&time, &rs.timer);
    reverse_stop_update(&rs, 19.8f, -300.0f, 0.0f, &time, false);
    EXPECT_TRUE(!reverse_stop_stop(&rs, &time));

    timer_expire(&time, &rs.timer, 2.4f);
    EXPECT_TRUE(!reverse_stop_stop(&rs, &time));
    timer_expire(&time, &rs.timer, 2.6f);
    EXPECT_TRUE(reverse_stop_stop(&rs, &time));

    reverse_stop_reset(&rs, 30.0f);
    reverse_stop_update(&rs, 30.5f, -300.0f, 0.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.target_setpoint, 0.0f);
    EXPECT_TRUE(!reverse_stop_active(&rs));

    rs.progress.value = 0.25f;
    rs.start_distance = 0.0f;
    rs.current_distance = 0.0f;
    rs.target_distance = 10.0f;
    rs.target_setpoint = 0.0f;
    timer_expire(&time, &rs.timer, 1.0f);
    reverse_stop_update(&rs, 0.1f, -300.0f, 20.0f, &time, true);
    EXPECT_TRUE(rs.timer == time.now);

    return true;
}

static bool test_reverse_stop_clears_progress_at_completed_distance(void) {
    ReverseStop rs;
    reverse_stop_init(&rs);
    reverse_stop_configure(&rs, 100.0f);

    Time time = {.now = 3000000u};
    reverse_stop_reset(&rs, 40.0f);
    reverse_stop_update(&rs, 39.7f, -300.0f, 0.0f, &time, true);
    EXPECT_TRUE(reverse_stop_active(&rs));
    EXPECT_TRUE(rs.target_distance < 0.0f);

    rs.progress.alpha = 1.0f;
    rs.current_distance = rs.target_distance;
    ema_reset(&rs.progress, 0.5f);
    timer_expire(&time, &rs.timer, 10.0f);
    reverse_stop_update(&rs, rs.start_distance + rs.target_distance, -300.0f, 4.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.progress.value, 1.0f);
    EXPECT_FLOAT_NEAR(rs.target_distance, 0.0f);
    EXPECT_FLOAT_NEAR(rs.current_distance, 0.0f);
    EXPECT_TRUE(rs.timer == time.now);

    systime_t timer = rs.timer;
    reverse_stop_update(&rs, rs.start_distance, -300.0f, 10.0f, &time, true);
    EXPECT_EQ_U32(rs.timer, timer);

    float completed_start = rs.start_distance;
    reverse_stop_update(&rs, completed_start + 0.5f, -300.0f, 0.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.start_distance, completed_start + 0.5f);
    EXPECT_TRUE(reverse_stop_active(&rs));
    EXPECT_FLOAT_NEAR(rs.target_distance, 0.0f);

    reverse_stop_update(&rs, rs.start_distance - 0.01f, -300.0f, 0.0f, &time, true);
    EXPECT_FLOAT_NEAR(rs.start_distance, completed_start + 0.5f);
    EXPECT_TRUE(reverse_stop_active(&rs));

    return true;
}
