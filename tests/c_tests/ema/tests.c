static bool test_ema_calculate_alpha_and_reset(void) {
    EMA ema = {0};
    ema_init(&ema);
    EXPECT_FLOAT_NEAR(ema.alpha, 0.0f);
    EXPECT_FLOAT_NEAR(ema.value, 0.0f);

    EXPECT_FLOAT_NEAR(ema_calculate_alpha(1000.0f, 100.0f), 0.375f);
    EXPECT_FLOAT_NEAR(ema_calculate_alpha(1.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_NEAR(ema_calculate_alpha(0.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_NEAR(ema_calculate_alpha(NAN, 100.0f), 0.0f);
    EXPECT_FLOAT_NEAR(ema_calculate_alpha(1.0f, NAN), 0.0f);
    EXPECT_FLOAT_NEAR(ema_calculate_alpha(1.0f, -100.0f), 0.0f);

    float alpha = ema_calculate_alpha(1.0f, 100.0f);
    EXPECT_TRUE(alpha > 0.0f);
    EXPECT_TRUE(alpha < 0.1f);

    float wrapped = ema_calculate_alpha_time_constant(1.0f / (2.0f * (float) M_PI), 100.0f);
    EXPECT_FLOAT_NEAR(wrapped, alpha);

    ema_configure(&ema, 1000.0f, 100.0f);
    EXPECT_FLOAT_NEAR(ema.alpha, 0.375f);

    ema_reset(&ema, 4.0f);
    EXPECT_FLOAT_NEAR(ema.value, 4.0f);
    ema_update(&ema, 8.0f);
    EXPECT_FLOAT_NEAR(ema.value, 5.5f);

    ema.alpha = 1.0f;
    ema_update(&ema, 2.0f);
    EXPECT_FLOAT_NEAR(ema.value, 2.0f);

    ema_update(&ema, NAN);
    EXPECT_FLOAT_NEAR(ema.value, 2.0f);
    ema_update(&ema, INFINITY);
    EXPECT_FLOAT_NEAR(ema.value, 2.0f);

    return true;
}
