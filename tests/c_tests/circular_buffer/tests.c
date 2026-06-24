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
