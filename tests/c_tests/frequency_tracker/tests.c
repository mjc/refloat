static bool test_frequency_tracker_nonpositive_dt(void) {
    FrequencyTracker ft;
    Time time = {.now = 1000u};
    frequency_tracker_init(&ft, 100.0f, &time);

    frequency_tracker_update(&ft, 0.01f);
    RED_EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    RED_EXPECT_TRUE(isfinite(ft.frequency.value));
    RED_EXPECT_TRUE(ft.frequency.value > 0.0f);

    float frequency_before = ft.frequency.value;
    frequency_tracker_update(&ft, 0.0f);
    RED_EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    RED_EXPECT_FLOAT_NEAR(ft.frequency.value, frequency_before);
    RED_EXPECT_TRUE(isfinite(ft.frequency.value));
    RED_EXPECT_TRUE(ft.frequency.value > 0.0f);

    frequency_tracker_update(&ft, -0.01f);
    RED_EXPECT_FLOAT_NEAR(ft.dt, 10.0f);
    RED_EXPECT_FLOAT_NEAR(ft.frequency.value, frequency_before);
    RED_EXPECT_TRUE(isfinite(ft.frequency.value));
    RED_EXPECT_TRUE(ft.frequency.value > 0.0f);

    return true;
}
