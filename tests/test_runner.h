#ifndef REFLOAT_TEST_RUNNER_H
#define REFLOAT_TEST_RUNNER_H

#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifndef TEST_FLOAT_EPS
#define TEST_FLOAT_EPS 1e-4f
#endif

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn fn;
    const char *xfail_reason;
} TestCase;
#define TEST_CASE(name_, fn_) {.name = (name_), .fn = (fn_), .xfail_reason = NULL}
#define XFAIL_CASE(name_, fn_, reason_) {.name = (name_), .fn = (fn_), .xfail_reason = (reason_)}

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            if (!current_test_is_xfail) {                                                          \
                fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr);          \
            }                                                                                      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_FLOAT_NEAR(actual, expected)                                                         \
    do {                                                                                           \
        float actual__ = (actual);                                                                 \
        float expected__ = (expected);                                                             \
        if (fabsf(actual__ - expected__) > TEST_FLOAT_EPS) {                                       \
            if (!current_test_is_xfail) {                                                          \
                fprintf(                                                                            \
                    stderr,                                                                         \
                    "%s:%d: expected %.6f, got %.6f for %s\n",                                    \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    (double) expected__,                                                           \
                    (double) actual__,                                                             \
                    #actual                                                                        \
                );                                                                                 \
            }                                                                                      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_U32(actual, expected)                                                                \
    do {                                                                                           \
        uint32_t actual__ = (actual);                                                              \
        uint32_t expected__ = (expected);                                                          \
        if (actual__ != expected__) {                                                              \
            if (!current_test_is_xfail) {                                                          \
                fprintf(                                                                            \
                    stderr,                                                                         \
                    "%s:%d: expected %" PRIu32 ", got %" PRIu32 " for %s\n",                    \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    expected__,                                                                    \
                    actual__,                                                                      \
                    #actual                                                                        \
                );                                                                                 \
            }                                                                                      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static inline int run_test_case(
    const TestCase *test_case,
    bool *current_test_is_xfail,
    size_t *passes,
    size_t *xfails,
    size_t *failures
) {
    *current_test_is_xfail = test_case->xfail_reason != NULL;
    bool ok = test_case->fn();
    *current_test_is_xfail = false;

    if (test_case->xfail_reason != NULL) {
        if (ok) {
            fprintf(stderr, "XPASS %s: %s\n", test_case->name, test_case->xfail_reason);
            ++(*failures);
            return 1;
        }

        printf("XFAIL %s: %s\n", test_case->name, test_case->xfail_reason);
        ++(*xfails);
        return 0;
    }

    if (!ok) {
        fprintf(stderr, "FAIL  %s\n", test_case->name);
        ++(*failures);
        return 1;
    }

    printf("PASS  %s\n", test_case->name);
    ++(*passes);
    return 0;
}

#define RUN_TEST_SUITE(summary_, tests_)                                                           \
    do {                                                                                           \
        size_t passes__ = 0;                                                                       \
        size_t xfails__ = 0;                                                                       \
        size_t failures__ = 0;                                                                     \
        for (size_t i__ = 0; i__ < sizeof(tests_) / sizeof((tests_)[0]); ++i__) {                  \
            run_test_case(&(tests_)[i__], &current_test_is_xfail, &passes__, &xfails__, &failures__); \
        }                                                                                          \
        printf(                                                                                    \
            "%s: %zu passed, %zu expected failures, %zu failures\n",                              \
            (summary_),                                                                            \
            passes__,                                                                              \
            xfails__,                                                                              \
            failures__                                                                             \
        );                                                                                         \
        return failures__ == 0 ? 0 : 1;                                                            \
    } while (0)

#endif
