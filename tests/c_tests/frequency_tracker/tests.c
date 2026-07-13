static bool test_frequency_tracker_nonpositive_dt(void) {
    FrequencyTracker ft;
    Time time = {.now = 1000u};
    frequency_tracker_init(&ft, 100.0f, &time);

    frequency_tracker_update(&ft, 0.01f);
    EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    EXPECT_TRUE(isfinite(ft.frequency.value));
    EXPECT_TRUE(ft.frequency.value > 0.0f);

    float frequency_before = ft.frequency.value;
    frequency_tracker_update(&ft, 0.0f);
    EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    EXPECT_FLOAT_NEAR(ft.frequency.value, frequency_before);
    EXPECT_TRUE(isfinite(ft.frequency.value));
    EXPECT_TRUE(ft.frequency.value > 0.0f);

    frequency_tracker_update(&ft, -0.01f);
    EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    EXPECT_FLOAT_NEAR(ft.frequency.value, frequency_before);
    EXPECT_TRUE(isfinite(ft.frequency.value));
    EXPECT_TRUE(ft.frequency.value > 0.0f);

    frequency_tracker_update(&ft, NAN);
    EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    EXPECT_FLOAT_NEAR(ft.frequency.value, frequency_before);

    return true;
}

static float frequency_tracker_last_frequency;
static size_t frequency_tracker_reconfig_calls;

static void frequency_tracker_reconfigure(float frequency) {
    frequency_tracker_last_frequency = frequency;
    ++frequency_tracker_reconfig_calls;
}

static bool test_frequency_tracker_reconfigures_after_settling(void) {
    FrequencyTracker tracker;
    Time time = {.now = 0};
    frequency_tracker_reconfig_calls = 0;
    frequency_tracker_last_frequency = 0.0f;
    frequency_tracker_init(&tracker, 100.0f, &time);

    tracker.frequency.value = 110.0f;
    time.now = SYSTEM_TICK_RATE_HZ + 1u;
    frequency_tracker_check(&tracker, false, &time, frequency_tracker_reconfigure);
    EXPECT_EQ_U32(frequency_tracker_reconfig_calls, 1u);
    EXPECT_FLOAT_NEAR(frequency_tracker_last_frequency, 110.0f);

    tracker.frequency.value = 111.0f;
    time.now += SYSTEM_TICK_RATE_HZ + 1u;
    frequency_tracker_check(&tracker, false, &time, frequency_tracker_reconfigure);
    EXPECT_EQ_U32(frequency_tracker_reconfig_calls, 1u);

    tracker.frequency.value = 120.0f;
    frequency_tracker_check(&tracker, true, &time, frequency_tracker_reconfigure);
    EXPECT_EQ_U32(frequency_tracker_reconfig_calls, 1u);

    tracker.frequency.value = 111.0f;
    time.now += SYSTEM_TICK_RATE_HZ + 1u;
    frequency_tracker_check(&tracker, true, &time, frequency_tracker_reconfigure);
    EXPECT_EQ_U32(frequency_tracker_reconfig_calls, 1u);

    tracker.frequency.value = 120.0f;
    time.now += SYSTEM_TICK_RATE_HZ + 1u;
    frequency_tracker_check(&tracker, true, &time, frequency_tracker_reconfigure);
    EXPECT_EQ_U32(frequency_tracker_reconfig_calls, 2u);
    EXPECT_FLOAT_NEAR(frequency_tracker_last_frequency, 120.0f);
    return true;
}
