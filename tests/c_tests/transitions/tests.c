static bool test_transitions_clamp_and_shape(void) {
    EXPECT_FLOAT_NEAR(smoothstep(-1.0f), 0.0f);
    EXPECT_FLOAT_NEAR(smoothstep(0.0f), 0.0f);
    EXPECT_FLOAT_NEAR(smoothstep(0.25f), 0.15625f);
    EXPECT_FLOAT_NEAR(smoothstep(0.5f), 0.5f);
    EXPECT_FLOAT_NEAR(smoothstep(1.0f), 1.0f);
    EXPECT_FLOAT_NEAR(smoothstep(2.0f), 1.0f);

    EXPECT_FLOAT_NEAR(smootherstep(-1.0f), 0.0f);
    EXPECT_FLOAT_NEAR(smootherstep(0.0f), 0.0f);
    EXPECT_FLOAT_NEAR(smootherstep(0.25f), 0.103515625f);
    EXPECT_FLOAT_NEAR(smootherstep(0.5f), 0.5f);
    EXPECT_FLOAT_NEAR(smootherstep(1.0f), 1.0f);
    EXPECT_FLOAT_NEAR(smootherstep(2.0f), 1.0f);
    return true;
}

static bool test_rate_limit_rejects_invalid_inputs(void) {
    float value = 2.0f;

    rate_limitf(&value, NAN, 1.0f);
    EXPECT_FLOAT_NEAR(value, 2.0f);
    rate_limitf(&value, 4.0f, NAN);
    EXPECT_FLOAT_NEAR(value, 2.0f);
    rate_limitf(&value, 4.0f, -1.0f);
    EXPECT_FLOAT_NEAR(value, 2.0f);

    rate_limitf(&value, 4.0f, 1.0f);
    EXPECT_FLOAT_NEAR(value, 3.0f);
    rate_limitf(&value, 4.0f, 1.0f);
    EXPECT_FLOAT_NEAR(value, 4.0f);
    return true;
}

static bool test_send_app_data_skips_send_after_overflow(void) {
    uint8_t buffer[1] = {0};
    size_t len = 0;

    vesc_if_fake_reset();
    SEND_APP_DATA(buffer, sizeof(buffer), sizeof(buffer) + 1);

    EXPECT_TRUE(vesc_if_fake_last_app_data(&len) == NULL);
    EXPECT_EQ_U32(len, 0u);
    return true;
}
