/*
 * Tlog timespec module test.
 *
 * Copyright (C) 2017 Red Hat
 *
 * This file is part of tlog.
 *
 * Tlog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Tlog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tlog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <tlog/timespec.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

int
main(void)
{
    const double fp_ts_max =
                    nextafter((double)LONG_MAX + (double)0.999999999, 0);
    const double fp_ts_min =
                    nextafter((double)LONG_MIN - (double)0.999999999, 0);
    double fp_ts_max_i;
    double fp_ts_max_f;
    double fp_ts_min_i;
    double fp_ts_min_f;
    long fp_max_s;
    long fp_max_ns;
    long fp_min_s;
    long fp_min_ns;
    bool passed = true;

    fp_ts_max_f = modf(fp_ts_max, &fp_ts_max_i);
    fp_max_s = (long)fp_ts_max_i;
    fp_max_ns = (long)(fp_ts_max_f * 1000000000);
    fp_ts_min_f = modf(fp_ts_min, &fp_ts_min_i);
    fp_min_s = (long)fp_ts_min_i;
    fp_min_ns = (long)(fp_ts_min_f * 1000000000);

#define SYMBOL_add '+'
#define SYMBOL_sub '-'
#define SYMBOL_mul '*'
#define SYMBOL_div '/'

#define EXPAND_TS(_sec, _nsec) ("(" #_sec ", " #_nsec ")")
#define TS(_sec, _nsec) {_sec, _nsec}

#define TEST(_func, _a, _b, _exp_res) \
    do {                                                        \
        bool __test_passed;                                     \
        const struct timespec __a = _a;                         \
        const struct timespec __b = _b;                         \
        const struct timespec __exp_res = _exp_res;             \
        struct timespec __res;                                  \
        tlog_timespec_##_func(&__a, &__b, &__res);              \
        __test_passed = (__res.tv_sec == __exp_res.tv_sec &&    \
                         __res.tv_nsec == __exp_res.tv_nsec);   \
        if (__test_passed) {                                    \
            fprintf(stderr,                                     \
                    "PASS %s %c %s == %s\n",                    \
                    EXPAND_##_a, SYMBOL_##_func, EXPAND_##_b,   \
                    EXPAND_##_exp_res);                         \
        } else {                                                \
            fprintf(stderr,                                     \
                    "FAIL %s %c %s = (%ld, %ld) != %s\n",       \
                    EXPAND_##_a, SYMBOL_##_func, EXPAND_##_b,   \
                    __res.tv_sec, __res.tv_nsec,                \
                    EXPAND_##_exp_res);                         \
        }                                                       \
        passed = __test_passed && passed;                       \
    } while (0)

    /*
     * Subtraction
     */
    TEST(sub, TS(0, 0), TS(0, 0), TS(0, 0));

    TEST(sub, TS(1, 0), TS(0, 0), TS(1, 0));
    TEST(sub, TS(1, 0), TS(1, 0), TS(0, 0));

    TEST(sub, TS(1, 1), TS(0, 0), TS(1, 1));
    TEST(sub, TS(1, 1), TS(1, 0), TS(0, 1));
    TEST(sub, TS(1, 1), TS(0, 1), TS(1, 0));
    TEST(sub, TS(1, 1), TS(1, 1), TS(0, 0));

    TEST(sub, TS(1, 0), TS(0, 500000000), TS(0, 500000000));
    TEST(sub, TS(1, 500000000), TS(1, 0), TS(0, 500000000));
    TEST(sub, TS(LONG_MAX, 999999999), TS(LONG_MAX, 999999999), TS(0, 0));
    TEST(sub, TS(1, 0), TS(0, 1), TS(0, 999999999));
    TEST(sub, TS(0, 0), TS(0, 1), TS(0, -1));
    TEST(sub, TS(0, 0), TS(1, 0), TS(-1, 0));
    TEST(sub, TS(1, 0), TS(1, 1), TS(0, -1));
    TEST(sub, TS(0, 0), TS(1, 1), TS(-1, -1));
    TEST(sub, TS(1, 0), TS(2, 1), TS(-1, -1));
    TEST(sub, TS(1, 1), TS(2, 2), TS(-1, -1));
    TEST(sub, TS(-1, 0), TS(0, 1), TS(-1, -1));
    TEST(sub, TS(-1, 0), TS(-1, 0), TS(0, 0));
    TEST(sub, TS(-1, 0), TS(-1, -1), TS(0, 1));
    TEST(sub, TS(-1, -1), TS(-1, -1), TS(0, 0));
    TEST(sub, TS(0, -1), TS(0, -1), TS(0, 0));
    TEST(sub, TS(0, 0), TS(LONG_MAX, 999999999),
              TS(LONG_MIN + 1, -999999999));
    TEST(sub, TS(LONG_MIN, -999999999), TS(0, 1), TS(LONG_MAX, 0));

    TEST(sub, TS(LONG_MIN / 2, 0), TS(LONG_MAX / 2 + 1, 0), TS(LONG_MIN, 0));
    TEST(sub, TS(LONG_MAX / 2, 499999999), TS(LONG_MIN / 2, -500000000),
              TS(LONG_MAX, 999999999));

    /*
     * Addition
     */
    TEST(add, TS(0, 0), TS(0, 0), TS(0, 0));

    TEST(add, TS(0, 0), TS(1, 0), TS(1, 0));
    TEST(add, TS(1, 0), TS(1, 0), TS(2, 0));

    TEST(add, TS(1, 1), TS(0, 0), TS(1, 1));
    TEST(add, TS(1, 1), TS(1, 0), TS(2, 1));
    TEST(add, TS(1, 1), TS(0, 1), TS(1, 2));

    TEST(add, TS(0, 999999999), TS(0, 0), TS(0, 999999999));
    TEST(add, TS(0, 999999999), TS(0, 1), TS(1, 0));
    TEST(add, TS(1, 999999999), TS(0, 1), TS(2, 0));
    TEST(add, TS(LONG_MAX, 999999999), TS(0, 1), TS(LONG_MIN, 0));
    TEST(add, TS(0, -1), TS(0, 1), TS(0, 0));
    TEST(add, TS(0, -1), TS(0, 2), TS(0, 1));
    TEST(add, TS(-1, 0), TS(0, 1), TS(0, -999999999));
    TEST(add, TS(-1, -1), TS(0, 1), TS(-1, 0));
    TEST(add, TS(-1, -1), TS(0, 2), TS(0, -999999999));
    TEST(add, TS(-1, 0), TS(0, -1), TS(-1, -1));

    TEST(add, TS(LONG_MAX / 2, 500000000), TS(LONG_MAX / 2, 500000000),
              TS(LONG_MAX, 0));
    TEST(add, TS(LONG_MIN / 2, 0), TS(LONG_MIN / 2, 0), TS(LONG_MIN, 0));

    /*
     * Multiplication
     */
    TEST(mul, TS(0, 0), TS(0, 0), TS(0, 0));
    TEST(mul, TS(0, 0), TS(1, 0), TS(0, 0));
    TEST(mul, TS(0, 0), TS(1, 100000000), TS(0, 0));
    TEST(mul, TS(1, 0), TS(1, 0), TS(1, 0));
    TEST(mul, TS(1, 0), TS(1, 1), TS(1, 1));
    TEST(mul, TS(1, 1), TS(1, 0), TS(1, 1));
    TEST(mul, TS(1, 0), TS(1, 100000000), TS(1, 100000000));
    TEST(mul, TS(1, 100000000), TS(1, 100000000), TS(1, 210000000));
    TEST(mul, TS(1, 0), TS(2, 0), TS(2, 0));
    TEST(mul, TS(2, 0), TS(2, 0), TS(4, 0));
    TEST(mul, TS(2, 0), TS(0, 500000000), TS(1, 0));
    TEST(mul, TS(2, 0), TS(1, 500000000), TS(3, 0));
    TEST(mul, TS(10, 0), TS(0, 100000000), TS(1, 0));

    TEST(mul, TS(LONG_MAX / 2, 0), TS(2, 0), TS(fp_max_s, fp_max_ns));
    TEST(mul, TS(LONG_MIN / 2, 0), TS(2, 0), TS(fp_min_s, fp_min_ns));
    TEST(mul, TS(LONG_MAX, 0), TS(0, 500000000), TS(LONG_MAX / 2 + 1, 0));
    TEST(mul, TS(LONG_MIN, 0), TS(0, 500000000), TS(LONG_MIN / 2, 0));

    TEST(mul, TS(LONG_MAX, 999999999), TS(LONG_MAX, 999999999),
              TS(fp_max_s, fp_max_ns));
    TEST(mul, TS(LONG_MAX, 999999999), TS(LONG_MIN, -999999999),
              TS(fp_min_s, fp_min_ns));
    TEST(mul, TS(LONG_MIN, -999999999), TS(LONG_MIN, -999999999),
              TS(fp_max_s, fp_max_ns));

    /* Time scaling values */
    TEST(mul, TS(0, 62500000), TS(2, 0), TS(0, 125000000));
    TEST(mul, TS(0, 125000000), TS(2, 0), TS(0, 250000000));
    TEST(mul, TS(0, 250000000), TS(2, 0), TS(0, 500000000));
    TEST(mul, TS(0, 500000000), TS(2, 0), TS(1, 0));
    TEST(mul, TS(1, 0), TS(2, 0), TS(2, 0));
    TEST(mul, TS(2, 0), TS(2, 0), TS(4, 0));
    TEST(mul, TS(4, 0), TS(2, 0), TS(8, 0));
    TEST(mul, TS(8, 0), TS(2, 0), TS(16, 0));

    /*
     * Division
     */
    TEST(div, TS(0, 0), TS(1, 0), TS(0, 0));
    TEST(div, TS(0, 0), TS(0, 1), TS(0, 0));
    TEST(div, TS(0, 0), TS(1, 1), TS(0, 0));
    TEST(div, TS(1, 0), TS(1, 0), TS(1, 0));
    TEST(div, TS(1, 1), TS(1, 0), TS(1, 1));
    TEST(div, TS(0, 1), TS(1, 0), TS(0, 1));
    TEST(div, TS(0, 999999999), TS(1, 0), TS(0, 999999999));
    TEST(div, TS(0, 1), TS(0, 500000000), TS(0, 2));

    TEST(div, TS(1000000000, 0), TS(1000000000, 0), TS(1, 0));
    TEST(div, TS(-1000000000, 0), TS(1000000000, 0), TS(-1, 0));
    TEST(div, TS(1000000000, 0), TS(-1000000000, 0), TS(-1, 0));
    TEST(div, TS(-1000000000, 0), TS(-1000000000, 0), TS(1, 0));

    TEST(div, TS(1000000000, 0), TS(1, 0), TS(1000000000, 0));
    TEST(div, TS(-1000000000, 0), TS(1, 0), TS(-1000000000, 0));
    TEST(div, TS(1000000000, 0), TS(-1, 0), TS(-1000000000, 0));
    TEST(div, TS(-1000000000, 0), TS(-1, 0), TS(1000000000, 0));

    TEST(div, TS(LONG_MAX, 999999999), TS(LONG_MAX, 999999999), TS(1, 0));
    TEST(div, TS(LONG_MIN, -999999999), TS(LONG_MIN, -999999999), TS(1, 0));
    TEST(div, TS(LONG_MAX, 999999999), TS(LONG_MIN+1, -999999999), TS(-1, 0));
    TEST(div, TS(LONG_MIN+1, -999999999), TS(LONG_MAX, 999999999), TS(-1, 0));

    TEST(div, TS(LONG_MAX, 999999999), TS(1, 0), TS(fp_max_s, fp_max_ns));
    TEST(div, TS(LONG_MIN, -999999999), TS(1, 0), TS(fp_min_s, fp_min_ns));

    /* Time scaling values */
    TEST(div, TS(16, 0), TS(2, 0), TS(8, 0));
    TEST(div, TS(8, 0), TS(2, 0), TS(4, 0));
    TEST(div, TS(4, 0), TS(2, 0), TS(2, 0));
    TEST(div, TS(2, 0), TS(2, 0), TS(1, 0));
    TEST(div, TS(1, 0), TS(2, 0), TS(0, 500000000));
    TEST(div, TS(0, 500000000), TS(2, 0), TS(0, 250000000));
    TEST(div, TS(0, 250000000), TS(2, 0), TS(0, 125000000));
    TEST(div, TS(0, 125000000), TS(2, 0), TS(0, 62500000));

    return !passed;
}
