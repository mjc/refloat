#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static inline uint32_t test_xorshift32(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static inline float test_random_unit(uint32_t *state) {
    return (float) (test_xorshift32(state) >> 8) * (1.0f / 16777216.0f);
}

static inline float test_random_range(uint32_t *state, float min, float max) {
    return min + (max - min) * test_random_unit(state);
}

#define TEST_EXPECT_TRUE_WITH_CONTEXT(seed, case_index, expr)                                      \
    do {                                                                                           \
        bool _test_ok = (expr);                                                                    \
        if (!_test_ok) {                                                                           \
            fprintf(                                                                               \
                stderr,                                                                            \
                "seed=0x%08x case=%zu: %s\n",                                                      \
                (unsigned) (seed),                                                                 \
                (size_t) (case_index),                                                             \
                #expr                                                                              \
            );                                                                                     \
        }                                                                                          \
        EXPECT_TRUE(_test_ok);                                                                     \
    } while (0)
