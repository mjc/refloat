#ifndef REFLOAT_TEST_RUNNER_H
#define REFLOAT_TEST_RUNNER_H

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Refloat defines its own time_t, so avoid including the host stdlib header here.
char *getenv(const char *name);

#ifndef SIGFPE
#define SIGFPE 8
#endif

#ifndef SIGSEGV
#define SIGSEGV 11
#endif

typedef void (*TestSignalHandler)(int);
TestSignalHandler signal(int signal_number, TestSignalHandler handler);

#ifndef TEST_FLOAT_EPS
#define TEST_FLOAT_EPS 1e-4f
#endif

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn fn;
} TestCase;

#define TEST_CASE(name_, fn_) {.name = (name_), .fn = (fn_)}

static inline void test_report_expr_failure(const char *file, int line, const char *expr) {
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expr);
}

static inline void test_report_u32_failure(
    const char *file, int line, const char *expr, uint32_t actual, uint32_t expected
) {
    fprintf(
        stderr,
        "%s:%d: expected %" PRIu32 ", got %" PRIu32 " for %s\n",
        file,
        line,
        expected,
        actual,
        expr
    );
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
    fprintf(
        stderr,
        "%s:%d: expected %.6f, got %.6f within %.6f for %s\n",
        file,
        line,
        expected,
        actual,
        eps,
        expr
    );
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
            test_report_u32_failure(__FILE__, __LINE__, #actual, actual__, expected__);            \
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
                __FILE__,                                                                          \
                __LINE__,                                                                          \
                #actual,                                                                           \
                (double) actual__,                                                                 \
                (double) expected__,                                                               \
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
            fprintf(                                                                               \
                stderr,                                                                            \
                "%s:%d: memory comparison failed for %s (%zu bytes)\n",                            \
                __FILE__,                                                                          \
                __LINE__,                                                                          \
                #actual,                                                                           \
                len__                                                                              \
            );                                                                                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

// One in-flight guard per test binary; these host tests run serially.
typedef bool (*TestSignalCallback)(void *ctx);

typedef struct {
    int signo;
    sigjmp_buf env;
} TestSignalGuard;

static TestSignalGuard *test_signal_guard_current = NULL;

static inline void test_signal_guard_handler(int signal_number) {
    TestSignalGuard *guard = test_signal_guard_current;
    if (guard != NULL && signal_number == guard->signo) {
        siglongjmp(guard->env, 1);
    }
}

static inline bool test_expect_no_signal(int signo, TestSignalCallback fn, void *ctx) {
    TestSignalGuard guard = {.signo = signo};
    void (*previous_handler)(int) = signal(signo, test_signal_guard_handler);
    bool ok = true;

    test_signal_guard_current = &guard;
    if (sigsetjmp(guard.env, 1) == 0) {
        ok = fn(ctx);
    } else {
        ok = false;
    }
    signal(signo, previous_handler);
    test_signal_guard_current = NULL;

    return ok;
}

static inline int run_test_case(const TestCase *test_case, size_t *passes, size_t *failures) {
    bool ok = test_case->fn();

    if (!ok) {
        fprintf(stderr, "FAIL  %s\n", test_case->name);
        ++(*failures);
        return 1;
    }

    printf("PASS  %s\n", test_case->name);
    ++(*passes);
    return 0;
}

static inline bool test_case_is_selected(const TestCase *test_case) {
    const char *selected = getenv("REFLOAT_TEST_CASE");
    return selected == NULL || selected[0] == '\0' || strcmp(selected, test_case->name) == 0;
}

#define RUN_TEST_SUITE(summary_, tests_)                                                           \
    do {                                                                                           \
        size_t passes__ = 0;                                                                       \
        size_t failures__ = 0;                                                                     \
        for (size_t i__ = 0; i__ < sizeof(tests_) / sizeof((tests_)[0]); ++i__) {                  \
            if (test_case_is_selected(&(tests_)[i__])) {                                           \
                run_test_case(&(tests_)[i__], &passes__, &failures__);                             \
            }                                                                                      \
        }                                                                                          \
        if (getenv("REFLOAT_TEST_CASE") != NULL && passes__ == 0 && failures__ == 0) {             \
            fprintf(                                                                               \
                stderr, "No test matched REFLOAT_TEST_CASE=%s\\n", getenv("REFLOAT_TEST_CASE")     \
            );                                                                                     \
            ++failures__;                                                                          \
        }                                                                                          \
        printf("%s: %zu passed, %zu failures\n", (summary_), passes__, failures__);                \
        return failures__ == 0 ? 0 : 1;                                                            \
    } while (0)

#endif
