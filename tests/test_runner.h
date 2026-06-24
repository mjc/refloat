#ifndef REFLOAT_TEST_RUNNER_H
#define REFLOAT_TEST_RUNNER_H

#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

static inline void test_report_expr_failure(const char *file, int line, const char *expr) {
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expr);
}

static inline void test_report_u32_failure(
    const char *file, int line, const char *expr, uint32_t actual, uint32_t expected
) {
    fprintf(stderr, "%s:%d: expected %" PRIu32 ", got %" PRIu32 " for %s\n",
        file, line, expected, actual, expr);
}

static inline void test_report_size_failure(
    const char *file, int line, const char *expr, size_t actual, size_t expected
) {
    fprintf(stderr, "%s:%d: expected %zu, got %zu for %s\n", file, line, expected, actual, expr);
}

static inline void test_report_ptr_failure(
    const char *file, int line, const char *expr, const void *actual, const void *expected
) {
    fprintf(stderr, "%s:%d: expected %p, got %p for %s\n", file, line, expected, actual, expr);
}

static inline void test_report_float_failure(
    const char *file, int line, const char *expr, double actual, double expected, double eps
) {
    fprintf(stderr, "%s:%d: expected %.6f, got %.6f within %.6f for %s\n",
        file, line, expected, actual, eps, expr);
}

#define EXPECT_TRUE(expr)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            test_report_expr_failure(__FILE__, __LINE__, #expr);                                   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ_U32(actual, expected)                                                            \
    do {                                                                                           \
        uint32_t actual__ = (actual);                                                              \
        uint32_t expected__ = (expected);                                                          \
        if (actual__ != expected__) {                                                              \
            test_report_u32_failure(__FILE__, __LINE__, #actual, actual__, expected__);           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_SIZE(actual, expected)                                                           \
    do {                                                                                           \
        size_t actual__ = (actual);                                                                \
        size_t expected__ = (expected);                                                            \
        if (actual__ != expected__) {                                                              \
            test_report_size_failure(__FILE__, __LINE__, #actual, actual__, expected__);           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_PTR(actual, expected)                                                            \
    do {                                                                                           \
        const void *actual__ = (const void *) (actual);                                            \
        const void *expected__ = (const void *) (expected);                                        \
        if (actual__ != expected__) {                                                              \
            test_report_ptr_failure(__FILE__, __LINE__, #actual, actual__, expected__);            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT_NEAR_EPS(actual, expected, eps)                                               \
    do {                                                                                           \
        float actual__ = (actual);                                                                 \
        float expected__ = (expected);                                                             \
        float eps__ = (eps);                                                                       \
        if (fabsf(actual__ - expected__) > eps__) {                                                \
            test_report_float_failure(                                                             \
                __FILE__, __LINE__, #actual, (double) actual__, (double) expected__,              \
                (double) eps__                                                                     \
            );                                                                                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define EXPECT_FLOAT_NEAR(actual, expected) EXPECT_FLOAT_NEAR_EPS(actual, expected, TEST_FLOAT_EPS)

#define EXPECT_MEM_EQ(actual, expected, len)                                                       \
    do {                                                                                           \
        const void *actual__ = (const void *) (actual);                                            \
        const void *expected__ = (const void *) (expected);                                        \
        size_t len__ = (len);                                                                      \
        if (memcmp(actual__, expected__, len__) != 0) {                                            \
            fprintf(stderr, "%s:%d: memory comparison failed for %s (%zu bytes)\n",               \
                __FILE__, __LINE__, #actual, len__);                                               \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define XEXPECT_TRUE(expr) EXPECT_TRUE(expr)

#define XEXPECT_FALSE(expr) XEXPECT_TRUE(!(expr))

#define XEXPECT_EQ_U32(actual, expected)                                                           \
    do {                                                                                           \
        uint32_t actual__ = (actual);                                                              \
        uint32_t expected__ = (expected);                                                          \
        if (actual__ != expected__) {                                                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define XEXPECT_EQ_SIZE(actual, expected)                                                          \
    do {                                                                                           \
        size_t actual__ = (actual);                                                                \
        size_t expected__ = (expected);                                                            \
        if (actual__ != expected__) {                                                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define XEXPECT_EQ_PTR(actual, expected)                                                           \
    do {                                                                                           \
        const void *actual__ = (const void *) (actual);                                            \
        const void *expected__ = (const void *) (expected);                                        \
        if (actual__ != expected__) {                                                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define XEXPECT_FLOAT_NEAR_EPS(actual, expected, eps)                                              \
    do {                                                                                           \
        float actual__ = (actual);                                                                 \
        float expected__ = (expected);                                                             \
        float eps__ = (eps);                                                                       \
        if (fabsf(actual__ - expected__) > eps__) {                                                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define XEXPECT_FLOAT_NEAR(actual, expected)                                                       \
    XEXPECT_FLOAT_NEAR_EPS(actual, expected, TEST_FLOAT_EPS)

#define XEXPECT_MEM_EQ(actual, expected, len)                                                      \
    do {                                                                                           \
        const void *actual__ = (const void *) (actual);                                            \
        const void *expected__ = (const void *) (expected);                                        \
        size_t len__ = (len);                                                                      \
        if (memcmp(actual__, expected__, len__) != 0) {                                            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK(expr) EXPECT_TRUE(expr)
#define CHECK_FLOAT_NEAR(actual, expected) EXPECT_FLOAT_NEAR(actual, expected)
#define CHECK_U32(actual, expected) EXPECT_EQ_U32(actual, expected)

#define XCHECK(expr) XEXPECT_TRUE(expr)
#define XCHECK_FLOAT_NEAR(actual, expected) XEXPECT_FLOAT_NEAR(actual, expected)
#define XCHECK_U32(actual, expected) XEXPECT_EQ_U32(actual, expected)

static inline int run_test_case(const TestCase *test_case, size_t *passes, size_t *xfails, size_t *failures) {
    bool ok = test_case->fn();

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
            run_test_case(&(tests_)[i__], &passes__, &xfails__, &failures__);                      \
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
