static bool test_sma_growth_transition_edges(void) {
    vesc_if_fake_reset();
    vesc_if_fake_fill_next_malloc(0x7f);

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 8.5f, 100.0f);
    EXPECT_TRUE(sma.array != NULL);
    EXPECT_TRUE(sma.n == 5);
    EXPECT_TRUE(sma.allocated_n == 6);

    for (uint8_t i = 0; i < sma.n; ++i) {
        sma_update(&sma, 10.0f);
    }
    EXPECT_FLOAT_NEAR(sma.value, 10.0f);

    uint8_t old_n = sma.n;
    sma_configure(&sma, 7.0f, 100.0f);
    EXPECT_TRUE(sma.n == old_n);
    EXPECT_TRUE(sma.new_n == 6);

    for (uint8_t i = 0; i < old_n; ++i) {
        sma_update(&sma, 10.0f);
    }
    EXPECT_TRUE(sma.n == 6);
    EXPECT_TRUE(sma.new_n == 0);
    EXPECT_TRUE(sma.idx == old_n);
    EXPECT_FLOAT_NEAR(sma.array[old_n], 10.0f);
    EXPECT_FLOAT_NEAR(sma.value, 10.0f);

    sma_update(&sma, 22.0f);
    EXPECT_TRUE(sma.idx == 0);
    EXPECT_FLOAT_NEAR(sma.value, 12.0f);

    sma_configure(&sma, 6.0f, 100.0f);
    EXPECT_TRUE(sma.new_n == 0);
    EXPECT_TRUE(sma.n == 6);

    sma_destroy(&sma);
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
    vesc_if_fake_reset();
    vesc_if_fake_fail_next_malloc();

    SMA sma;
    sma_init(&sma);
    sma_configure(&sma, 1.0f, 100.0f);
    EXPECT_TRUE(sma.array == NULL);
    EXPECT_EQ_U32(sma.n, 0u);

    SmaUpdateGuard guard = {.sma = &sma, .value = 12.0f};
    EXPECT_TRUE(test_expect_no_signal(SIGSEGV, run_sma_update, &guard));

    EXPECT_FLOAT_NEAR(sma.value, 0.0f);
    EXPECT_EQ_U32(sma.idx, 0u);
    EXPECT_EQ_U32(vesc_if_fake_malloc_calls(), 1u);
    EXPECT_EQ_U32(vesc_if_fake_free_calls(), 0u);

    return true;
}

static bool test_circular_buffer_pop_index(void) {
    BufferItem storage[3] = {{0}};
    CircularBuffer cb;
    circular_buffer_init(&cb, sizeof(BufferItem), 3, storage);

    BufferItem a = {1, 11};
    BufferItem b = {2, 22};
    BufferItem c = {3, 33};
    BufferItem out = {0};

    circular_buffer_push(&cb, &a);
    circular_buffer_push(&cb, &b);
    circular_buffer_push(&cb, &c);

    EXPECT_TRUE(circular_buffer_pop(&cb, 1, &out));
    EXPECT_TRUE(buffer_item_eq(out, b));
    EXPECT_TRUE(circular_buffer_size(&cb) == 2);
    EXPECT_TRUE(circular_buffer_get(&cb, 0, &out));
    EXPECT_TRUE(buffer_item_eq(out, a));
    EXPECT_TRUE(circular_buffer_get(&cb, 1, &out));
    EXPECT_TRUE(buffer_item_eq(out, c));

    return true;
}
