typedef struct {
    SMA sma;
} SmaFixture;

static void sma_fixture_start(SmaFixture *fixture) {
    vesc_if_fake_reset();
    fixture->sma = (SMA) {0};
    sma_init(&fixture->sma);
}

static void sma_fixture_destroy(SmaFixture *fixture) {
    sma_destroy(&fixture->sma);
}

static bool test_sma_growth_transition_edges(void) {
    SmaFixture fixture;
    sma_fixture_start(&fixture);
    vesc_if_fake_fill_next_malloc(0x7f);

    sma_configure(&fixture.sma, 8.5f, 100.0f);
    EXPECT_TRUE(fixture.sma.array != NULL);
    EXPECT_TRUE(fixture.sma.n == 5);
    EXPECT_TRUE(fixture.sma.allocated_n == 6);

    for (uint8_t i = 0; i < fixture.sma.n; ++i) {
        sma_update(&fixture.sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(fixture.sma.value, 10.0f);

    uint8_t old_n = fixture.sma.n;
    sma_configure(&fixture.sma, 7.0f, 100.0f);
    EXPECT_TRUE(fixture.sma.n == old_n);
    EXPECT_TRUE(fixture.sma.new_n == 6);

    for (uint8_t i = 0; i < old_n; ++i) {
        sma_update(&fixture.sma, 10.0f);
    }
    EXPECT_TRUE(fixture.sma.n == 6);
    EXPECT_TRUE(fixture.sma.new_n == 0);
    EXPECT_TRUE(fixture.sma.idx == old_n);
    EXPECT_FLOAT_NEAR(fixture.sma.array[old_n], 10.0f);
    EXPECT_FLOAT_NEAR(fixture.sma.value, 10.0f);

    sma_update(&fixture.sma, 22.0f);
    EXPECT_TRUE(fixture.sma.idx == 0);
    EXPECT_FLOAT_NEAR(fixture.sma.value, 12.0f);

    sma_configure(&fixture.sma, 6.0f, 100.0f);
    EXPECT_TRUE(fixture.sma.new_n == 0);
    EXPECT_TRUE(fixture.sma.n == 6);

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
