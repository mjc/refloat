static bool test_biquad_filter_modes_and_reset(void) {
    Biquad biquad = {0};
    biquad_init(&biquad);
    EXPECT_FLOAT_NEAR(biquad.value, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.z1, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.z2, 0.0f);

    biquad_configure(&biquad, BQ_LOWPASS, 10.0f, 100.0f);
    EXPECT_TRUE(isfinite(biquad.a0));
    EXPECT_FLOAT_NEAR(biquad.a1, 2.0f * biquad.a0);
    EXPECT_FLOAT_NEAR(biquad.a2, biquad.a0);
    biquad_update(&biquad, 1.0f);
    EXPECT_TRUE(isfinite(biquad.value));
    EXPECT_TRUE(biquad.value > 0.0f);

    biquad_reset(&biquad);
    EXPECT_FLOAT_NEAR(biquad.value, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.z1, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.z2, 0.0f);

    biquad_configure(&biquad, BQ_HIGHPASS, 10.0f, 100.0f);
    EXPECT_TRUE(isfinite(biquad.a0));
    EXPECT_FLOAT_NEAR(biquad.a1, -2.0f * biquad.a0);
    EXPECT_FLOAT_NEAR(biquad.a2, biquad.a0);
    biquad_update(&biquad, 1.0f);
    EXPECT_TRUE(isfinite(biquad.value));
    EXPECT_TRUE(biquad.value > 0.0f);

    biquad_configure(&biquad, BQ_LOWPASS, 10.0f, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.a0, 1.0f);
    EXPECT_FLOAT_NEAR(biquad.a1, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.b1, 0.0f);

    biquad_configure(&biquad, (BiquadType) 99, 10.0f, 100.0f);
    EXPECT_FLOAT_NEAR(biquad.a0, 1.0f);
    EXPECT_FLOAT_NEAR(biquad.a1, 0.0f);
    EXPECT_FLOAT_NEAR(biquad.b2, 0.0f);

    const struct {
        float cutoff;
        float update;
    } invalid[] = {
        {NAN, 100.0f},
        {-1.0f, 100.0f},
        {10.0f, NAN},
        {10.0f, -1.0f},
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        biquad_configure(&biquad, BQ_LOWPASS, invalid[i].cutoff, invalid[i].update);
        EXPECT_FLOAT_NEAR(biquad.a0, 1.0f);
        EXPECT_FLOAT_NEAR(biquad.a1, 0.0f);
        EXPECT_FLOAT_NEAR(biquad.b2, 0.0f);
    }
    return true;
}
