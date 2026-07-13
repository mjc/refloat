typedef struct {
    SMA sma;
} SmaFixture;

static void sma_fixture_start(SmaFixture *fixture) {
    vesc_if_fake_reset();
    fixture->sma = (SMA){0};
    sma_init(&fixture->sma);
}

static void sma_fixture_destroy(SmaFixture *fixture) {
    sma_destroy(&fixture->sma);
}

static bool sma_prime_steady_output(SmaFixture *fixture, float value) {
    sma_configure(&fixture->sma, 8.5f, 100.0f);
    for (uint8_t i = 0; i < 5; ++i) {
        sma_update(&fixture->sma, value);
    }
    EXPECT_FLOAT_NEAR(fixture->sma.value, value);
    return true;
}

static bool test_sma_applies_first_pending_resize_without_output_jump(void) {
    SmaFixture fixture;
    sma_fixture_start(&fixture);
    vesc_if_fake_fill_next_malloc(0x7f);

    EXPECT_TRUE(sma_prime_steady_output(&fixture, 10.0f));
    sma_configure(&fixture.sma, 7.0f, 100.0f);
    sma_configure(&fixture.sma, 20.0f, 100.0f);

    for (uint8_t i = 0; i < 5; ++i) {
        sma_update(&fixture.sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(fixture.sma.value, 10.0f);

    sma_update(&fixture.sma, 22.0f);
    EXPECT_FLOAT_NEAR(fixture.sma.value, 12.0f);

    sma_fixture_destroy(&fixture);
    EXPECT_TRUE(vesc_if_fake_free_calls() == 1);

    return true;
}

typedef struct {
    SMA *sma;
    float value;
} SmaUpdateGuard;

static bool run_sma_update(void *ctx) {
    SmaUpdateGuard *guard = ctx;
    sma_update(guard->sma, guard->value);
    return true;
}

static bool test_sma_allocation_failure_update(void) {
    SmaFixture fixture;
    sma_fixture_start(&fixture);
    vesc_if_fake_fail_next_malloc();

    sma_configure(&fixture.sma, 1.0f, 100.0f);
    EXPECT_TRUE(fixture.sma.array == NULL);
    EXPECT_EQ_U32(fixture.sma.n, 0u);

    SmaUpdateGuard guard = {.sma = &fixture.sma, .value = 12.0f};
    EXPECT_TRUE(test_expect_no_signal(SIGSEGV, run_sma_update, &guard));

    EXPECT_FLOAT_NEAR(fixture.sma.value, 0.0f);
    EXPECT_EQ_U32(fixture.sma.idx, 0u);
    EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_free_calls(), 0u);

    return true;
}

static bool test_sma_rejects_invalid_inputs(void) {
    SmaFixture fixture;
    sma_fixture_start(&fixture);

    sma_configure(&fixture.sma, 8.5f, 100.0f);
    sma_update(&fixture.sma, 10.0f);
    uint8_t expected_n = fixture.sma.n;
    uint8_t expected_idx = fixture.sma.idx;
    float expected_value = fixture.sma.value;

    sma_update(&fixture.sma, NAN);
    EXPECT_EQ_U32(fixture.sma.n, expected_n);
    EXPECT_EQ_U32(fixture.sma.idx, expected_idx);
    EXPECT_FLOAT_NEAR(fixture.sma.value, expected_value);
    EXPECT_TRUE(isfinite(fixture.sma.value));

    sma_configure(&fixture.sma, NAN, 100.0f);
    EXPECT_EQ_U32(fixture.sma.n, expected_n);
    sma_configure(&fixture.sma, -1.0f, 100.0f);
    EXPECT_EQ_U32(fixture.sma.n, expected_n);
    sma_configure(&fixture.sma, 8.5f, NAN);
    EXPECT_EQ_U32(fixture.sma.n, expected_n);
    sma_configure(&fixture.sma, 8.5f, 0.0f);
    EXPECT_EQ_U32(fixture.sma.n, expected_n);

    sma_fixture_destroy(&fixture);
    return true;
}

static bool test_sma_preserves_steady_output_across_resize(void) {
    SmaFixture fixture;
    sma_fixture_start(&fixture);

    EXPECT_TRUE(sma_prime_steady_output(&fixture, 10.0f));

    sma_configure(&fixture.sma, 20.0f, 100.0f);
    for (uint8_t i = 0; i < 5; ++i) {
        sma_update(&fixture.sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(fixture.sma.value, 10.0f);

    sma_configure(&fixture.sma, 5.0f, 100.0f);
    for (uint8_t i = 0; i < 2; ++i) {
        sma_update(&fixture.sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(fixture.sma.value, 10.0f);

    sma_fixture_destroy(&fixture);
    return true;
}
