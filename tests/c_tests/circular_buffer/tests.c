#include "c_tests/test_support.h"

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

static bool test_circular_buffer_zero_capacity_is_inert(void) {
    CircularBuffer cb = {0};
    int storage[2] = {0};
    int value = 42;
    int output = 0;
    circular_buffer_init(&cb, sizeof(value), 0, NULL);
    circular_buffer_push(&cb, &value);
    EXPECT_EQ_U32(circular_buffer_size(&cb), 0u);
    EXPECT_TRUE(!circular_buffer_get(&cb, 0, &output));

    circular_buffer_init(&cb, 0, 2, storage);
    circular_buffer_push(&cb, &value);
    EXPECT_TRUE(!circular_buffer_get(&cb, 0, &output));

    circular_buffer_init(&cb, sizeof(value), 2, NULL);
    circular_buffer_push(&cb, &value);
    EXPECT_TRUE(!circular_buffer_get(&cb, 0, &output));

    circular_buffer_init(&cb, sizeof(value), 2, storage);
    circular_buffer_push(&cb, NULL);
    EXPECT_TRUE(!circular_buffer_get(&cb, 0, &output));
    circular_buffer_push(&cb, &value);
    EXPECT_TRUE(!circular_buffer_get(&cb, 0, NULL));
    EXPECT_TRUE(!circular_buffer_get(&cb, 1, &output));
    EXPECT_TRUE(!circular_buffer_pop(&cb, 1, &output));

    circular_buffer_clear(&cb);
    circular_buffer_iterate(&cb, NULL, NULL);
    return true;
}

static bool test_circular_buffer_push_get_order_property(void) {
    enum {
        CAPACITY = 4,
        CASES = 256
    };
    BufferItem storage[CAPACITY] = {{0}};
    BufferItem expected[CAPACITY] = {{0}};
    CircularBuffer cb;
    uint32_t seed = 0xc1a7u;
    size_t count = 0;

    circular_buffer_init(&cb, sizeof(BufferItem), CAPACITY, storage);

    for (size_t i = 0; i < CASES; ++i) {
        uint32_t choice = test_xorshift32(&seed);

        if (count == CAPACITY || (count > 0 && (choice & 1u) == 0u)) {
            BufferItem out = {0};
            EXPECT_TRUE(circular_buffer_pop(&cb, 0, &out));
            EXPECT_TRUE(buffer_item_eq(out, expected[0]));
            memmove(expected, expected + 1, (count - 1) * sizeof(expected[0]));
            --count;
        } else {
            BufferItem item = {
                .lo = (uint8_t) choice,
                .hi = (uint8_t) (choice >> 8),
            };
            circular_buffer_push(&cb, &item);
            expected[count++] = item;
        }

        EXPECT_EQ_SIZE(circular_buffer_size(&cb), count);
        for (size_t index = 0; index < count; ++index) {
            BufferItem actual = {0};
            EXPECT_TRUE(circular_buffer_get(&cb, index, &actual));
            EXPECT_TRUE(buffer_item_eq(actual, expected[index]));
        }
    }

    return true;
}
