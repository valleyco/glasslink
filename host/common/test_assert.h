#pragma once

#include <stdio.h>
#include <stdint.h>

static int g_test_failures = 0;
static int g_test_passes = 0;

#define ASSERT_EQ_INT(expected, actual)                                          \
    do {                                                                         \
        int _e = (int)(expected);                                                \
        int _a = (int)(actual);                                                  \
        if (_e != _a) {                                                          \
            fprintf(stderr, "FAIL %s:%d: expected %d, got %d\n", __FILE__,       \
                    __LINE__, _e, _a);                                           \
            g_test_failures++;                                                   \
        } else {                                                                 \
            g_test_passes++;                                                     \
        }                                                                        \
    } while (0)

#define ASSERT_EQ_U16(expected, actual)                                          \
    do {                                                                         \
        unsigned _e = (unsigned)(expected);                                      \
        unsigned _a = (unsigned)(actual);                                        \
        if (_e != _a) {                                                          \
            fprintf(stderr, "FAIL %s:%d: expected 0x%04x, got 0x%04x\n",         \
                    __FILE__, __LINE__, _e, _a);                                 \
            g_test_failures++;                                                   \
        } else {                                                                 \
            g_test_passes++;                                                     \
        }                                                                        \
    } while (0)

#define ASSERT_TRUE(cond)                                                        \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            g_test_failures++;                                                   \
        } else {                                                                 \
            g_test_passes++;                                                     \
        }                                                                        \
    } while (0)

static inline int test_report(void)
{
    printf("total asserts: %d  passed: %d  failed: %d\n",
           g_test_passes + g_test_failures, g_test_passes, g_test_failures);
    return g_test_failures ? 1 : 0;
}
