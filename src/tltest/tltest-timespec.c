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

typedef void (*op_fn)(const struct timespec *a,
                      const struct timespec *b,
                      struct timespec *res);
struct op {
    const char *sym;
    op_fn       fn;
};

static bool
op_list_test(const char *file, int line,
             const struct op *op_list, size_t op_num,
             struct timespec a, struct timespec b, struct timespec exp_res)
{
    bool all_passed = true;
    size_t i;
    bool passed;
    struct timespec res;

    for (i = 0; i < op_num; i++) {
        op_list[i].fn(&a, &b, &res);
        passed = (res.tv_sec == exp_res.tv_sec && res.tv_nsec == exp_res.tv_nsec);
        if (passed) {
            fprintf(stderr,
                    "PASS %s:%d " TLOG_TIMESPEC_FMT " %s " TLOG_TIMESPEC_FMT
                    " == " TLOG_TIMESPEC_FMT "\n",
                    file, line,
                    TLOG_TIMESPEC_ARG(&a), op_list[i].sym,
                    TLOG_TIMESPEC_ARG(&b),
                    TLOG_TIMESPEC_ARG(&exp_res));
        } else {
            fprintf(stderr,
                    "FAIL %s:%d " TLOG_TIMESPEC_FMT " %s " TLOG_TIMESPEC_FMT
                    " = " TLOG_TIMESPEC_FMT " != " TLOG_TIMESPEC_FMT "\n",
                    file, line,
                    TLOG_TIMESPEC_ARG(&a), op_list[i].sym,
                    TLOG_TIMESPEC_ARG(&b),
                    TLOG_TIMESPEC_ARG(&res), TLOG_TIMESPEC_ARG(&exp_res));
        }
        all_passed = all_passed && passed;
    }

    return all_passed;
}

int
main(void)
{
    const struct op add[]       = {{"   +", tlog_timespec_add}};
    const struct op sub[]       = {{"   -", tlog_timespec_sub}};
    const struct op cap_add[]   = {{"cap+", tlog_timespec_cap_add}};
    const struct op cap_sub[]   = {{"cap-", tlog_timespec_cap_sub}};
    const struct op fp_add[]    = {{" fp+", tlog_timespec_fp_add}};
    const struct op fp_sub[]    = {{" fp-", tlog_timespec_fp_sub}};
    const struct op fp_mul[]    = {{" fp*", tlog_timespec_fp_mul}};
    const struct op fp_div[]    = {{" fp/", tlog_timespec_fp_div}};
    const struct op int_add[]   = {{"   +", tlog_timespec_add},
                                   {"cap+", tlog_timespec_cap_add}};
    const struct op int_sub[]   = {{"   -", tlog_timespec_sub},
                                   {"cap-", tlog_timespec_cap_sub}};
    const struct op all_add[]   = {{"   +", tlog_timespec_add},
                                   {"cap+", tlog_timespec_cap_add},
                                   {" fp+", tlog_timespec_fp_add}};
    const struct op all_sub[]   = {{"   -", tlog_timespec_sub},
                                   {"cap-", tlog_timespec_cap_sub},
                                   {" fp-", tlog_timespec_fp_sub}};

    struct timespec max_fp;
    struct timespec min_fp;
    bool passed = true;

    tlog_timespec_from_fp(TLOG_TIMESPEC_FP_MAX, &max_fp);
    tlog_timespec_from_fp(TLOG_TIMESPEC_FP_MIN, &min_fp);

#define TS(_sec, _nsec) (struct timespec){_sec, _nsec}

#define TEST(_op_list, _a, _b, _exp_res) \
    do {                                                      \
        passed = op_list_test(__FILE__, __LINE__, (_op_list), \
                              TLOG_ARRAY_SIZE(_op_list),      \
                              _a, _b, _exp_res) && passed;    \
    } while (0)

    /*
     * Subtraction
     */
    TEST(all_sub, TS(0, 0), TS(0, 0), TS(0, 0));

    TEST(all_sub, TS(1, 0), TS(0, 0), TS(1, 0));
    TEST(all_sub, TS(1, 0), TS(1, 0), TS(0, 0));

    TEST(all_sub, TS(1, 1), TS(0, 0), TS(1, 1));
    TEST(all_sub, TS(1, 1), TS(1, 0), TS(0, 1));
    TEST(all_sub, TS(1, 1), TS(0, 1), TS(1, 0));
    TEST(all_sub, TS(1, 1), TS(1, 1), TS(0, 0));

    TEST(all_sub, TS(1, 0), TS(0, 500000000), TS(0, 500000000));
    TEST(all_sub, TS(1, 500000000), TS(1, 0), TS(0, 500000000));
    TEST(all_sub, tlog_timespec_max, tlog_timespec_max, TS(0, 0));
    TEST(all_sub, TS(1, 0), TS(0, 1), TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1));
    TEST(all_sub, TS(0, 0), TS(0, 1), TS(0, -1));
    TEST(all_sub, TS(0, 0), TS(1, 0), TS(-1, 0));
    TEST(all_sub, TS(1, 0), TS(1, 1), TS(0, -1));
    TEST(all_sub, TS(0, 0), TS(1, 1), TS(-1, -1));
    TEST(all_sub, TS(1, 0), TS(2, 1), TS(-1, -1));
    TEST(all_sub, TS(1, 1), TS(2, 2), TS(-1, -1));
    TEST(all_sub, TS(-1, 0), TS(0, 1), TS(-1, -1));
    TEST(all_sub, TS(-1, 0), TS(0, -1), TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1));
    TEST(all_sub, TS(-1, 0), TS(0, 1), TS(-1, -1));
    TEST(all_sub, TS(-1, 0), TS(-1, 0), TS(0, 0));
    TEST(all_sub, TS(-1, 0), TS(-1, -1), TS(0, 1));
    TEST(all_sub, TS(-1, -1), TS(-1, -1), TS(0, 0));
    TEST(all_sub, TS(0, -1), TS(0, -1), TS(0, 0));
    TEST(all_sub, TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), TS(0, 1), TS(-1, 0));
      TEST(all_sub, TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), TS(0, 2), TS(-1, -1));
    TEST(all_sub, TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(0, -1), TS(1, 0));
    TEST(all_sub, TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(0, -2), TS(1, 1));
    TEST(all_sub, TS(1, 500000000), TS(2, 0), TS(0, -500000000));
    TEST(all_sub, TS(-1, -500000000), TS(-2, 0), TS(0, 500000000));
    TEST(int_sub, TS(0, 0), tlog_timespec_max, TS(LONG_MIN + 1, -TLOG_TIMESPEC_NSEC_PER_SEC + 1));

    TEST(sub, tlog_timespec_min, TS(0, 1), TS(LONG_MAX, 0));
    TEST(cap_sub, tlog_timespec_min, TS(0, 1), tlog_timespec_min);
    TEST(fp_sub, tlog_timespec_min, TS(0, 1), min_fp);

    TEST(int_sub, TS(LONG_MIN / 2, 0), TS(LONG_MAX / 2 + 1, 0), TS(LONG_MIN, 0));
    TEST(fp_sub, TS(LONG_MIN / 2, 0), TS(LONG_MAX / 2 + 1, 0), min_fp);
    TEST(int_sub, TS(LONG_MAX / 2, 499999999), TS(LONG_MIN / 2, -500000000),
                  tlog_timespec_max);
    TEST(fp_sub, TS(LONG_MAX / 2, 499999999), TS(LONG_MIN / 2, -500000000),
                 max_fp);

    TEST(cap_sub, tlog_timespec_min, tlog_timespec_max, tlog_timespec_min);
    TEST(cap_sub, tlog_timespec_max, tlog_timespec_min, tlog_timespec_max);

    TEST(cap_sub, tlog_timespec_max, TS(0, -2), tlog_timespec_max);
    TEST(cap_sub, tlog_timespec_min, TS(0, 2), tlog_timespec_min);

    TEST(fp_sub, tlog_timespec_min, tlog_timespec_max, min_fp);
    TEST(fp_sub, tlog_timespec_max, tlog_timespec_min, max_fp);

    /*
     * Addition
     */
    TEST(all_add, TS(0, 0), TS(0, 0), TS(0, 0));

    TEST(all_add, TS(0, 0), TS(1, 0), TS(1, 0));
    TEST(all_add, TS(1, 0), TS(1, 0), TS(2, 0));

    TEST(all_add, TS(1, 1), TS(0, 0), TS(1, 1));
    TEST(all_add, TS(1, 1), TS(1, 0), TS(2, 1));
    TEST(all_add, TS(1, 1), TS(0, 1), TS(1, 2));

    TEST(all_add, TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(0, 0), TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1));
    TEST(all_add, TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(0, 1), TS(1, 0));
    TEST(all_add, TS(1, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(0, 1), TS(2, 0));
    TEST(add, tlog_timespec_max, TS(0, 1), TS(LONG_MIN, 0));
    TEST(cap_add, tlog_timespec_max, TS(0, 1), tlog_timespec_max);
    TEST(fp_add, tlog_timespec_max, TS(0, 1), max_fp);
    TEST(all_add, TS(0, -1), TS(0, 1), TS(0, 0));
    TEST(all_add, TS(0, -1), TS(0, 2), TS(0, 1));
    TEST(all_add, TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), TS(0, -1), TS(-1, 0));
    TEST(all_add, TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), TS(0, -2), TS(-1, -1));
    TEST(all_add, TS(-1, 0), TS(0, 1), TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1));
    TEST(all_add, TS(-1, 0), TS(0, -1), TS(-1, -1));
    TEST(all_add, TS(-1, 0), TS(0, 1), TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1));
    TEST(all_add, TS(-1, -1), TS(0, 1), TS(-1, 0));
    TEST(all_add, TS(-1, -1), TS(0, 2), TS(0, -TLOG_TIMESPEC_NSEC_PER_SEC + 1));
    TEST(all_add, TS(-1, 0), TS(0, -1), TS(-1, -1));
    TEST(all_add, TS(-1, -500000000), TS(2, 0), TS(0, 500000000));
    TEST(all_add, TS(1, 500000000), TS(-2, 0), TS(0, -500000000));

    TEST(int_add, TS(LONG_MAX / 2, 500000000), TS(LONG_MAX / 2, 500000000),
                  TS(LONG_MAX, 0));
    TEST(fp_add, TS(LONG_MAX / 2, 500000000), TS(LONG_MAX / 2, 500000000),
                 max_fp);
    TEST(int_add, TS(LONG_MIN / 2, 0), TS(LONG_MIN / 2, 0), TS(LONG_MIN, 0));
    TEST(fp_add, TS(LONG_MIN / 2, 0), TS(LONG_MIN / 2, 0), min_fp);

    TEST(add, tlog_timespec_max, TS(0, 1), TS(LONG_MIN, 0));
    TEST(cap_add, tlog_timespec_max, TS(0, 1), tlog_timespec_max);
    TEST(fp_add, tlog_timespec_max, TS(0, 1), max_fp);

    TEST(add, tlog_timespec_min, TS(0, -1), TS(LONG_MAX, 0));
    TEST(cap_add, tlog_timespec_min, TS(0, -1), tlog_timespec_min);
    TEST(fp_add, tlog_timespec_min, TS(0, -1), min_fp);

    TEST(cap_add, tlog_timespec_max, tlog_timespec_max, tlog_timespec_max);
    TEST(cap_add, tlog_timespec_min, tlog_timespec_min, tlog_timespec_min);

    TEST(cap_add, tlog_timespec_max, TS(0, 2), tlog_timespec_max);
    TEST(cap_add, tlog_timespec_min, TS(0, -2), tlog_timespec_min);

    TEST(fp_add, tlog_timespec_max, tlog_timespec_max, max_fp);
    TEST(fp_add, tlog_timespec_min, tlog_timespec_min, min_fp);

    /*
     * Multiplication
     */
    TEST(fp_mul, TS(0, 0), TS(0, 0), TS(0, 0));
    TEST(fp_mul, TS(0, 0), TS(1, 0), TS(0, 0));
    TEST(fp_mul, TS(0, 0), TS(1, 100000000), TS(0, 0));
    TEST(fp_mul, TS(1, 0), TS(1, 0), TS(1, 0));
    TEST(fp_mul, TS(1, 0), TS(1, 1), TS(1, 1));
    TEST(fp_mul, TS(1, 1), TS(1, 0), TS(1, 1));
    TEST(fp_mul, TS(1, 0), TS(1, 100000000), TS(1, 100000000));
    TEST(fp_mul, TS(1, 100000000), TS(1, 100000000), TS(1, 210000000));
    TEST(fp_mul, TS(1, 0), TS(2, 0), TS(2, 0));
    TEST(fp_mul, TS(2, 0), TS(2, 0), TS(4, 0));
    TEST(fp_mul, TS(2, 0), TS(0, 500000000), TS(1, 0));
    TEST(fp_mul, TS(2, 0), TS(1, 500000000), TS(3, 0));
    TEST(fp_mul, TS(10, 0), TS(0, 100000000), TS(1, 0));

    TEST(fp_mul, TS(LONG_MAX / 2, 0), TS(2, 0), max_fp);
    TEST(fp_mul, TS(LONG_MIN / 2, 0), TS(2, 0), min_fp);
    TEST(fp_mul, TS(LONG_MAX, 0), TS(0, 500000000), TS(LONG_MAX / 2 + 1, 0));
    TEST(fp_mul, TS(LONG_MIN, 0), TS(0, 500000000), TS(LONG_MIN / 2, 0));

    TEST(fp_mul, tlog_timespec_max, tlog_timespec_max, max_fp);
    TEST(fp_mul, tlog_timespec_max, tlog_timespec_min, min_fp);
    TEST(fp_mul, tlog_timespec_min, tlog_timespec_min, max_fp);

    /* Time scaling values */
    TEST(fp_mul, TS(0, 62500000), TS(2, 0), TS(0, 125000000));
    TEST(fp_mul, TS(0, 125000000), TS(2, 0), TS(0, 250000000));
    TEST(fp_mul, TS(0, 250000000), TS(2, 0), TS(0, 500000000));
    TEST(fp_mul, TS(0, 500000000), TS(2, 0), TS(1, 0));
    TEST(fp_mul, TS(1, 0), TS(2, 0), TS(2, 0));
    TEST(fp_mul, TS(2, 0), TS(2, 0), TS(4, 0));
    TEST(fp_mul, TS(4, 0), TS(2, 0), TS(8, 0));
    TEST(fp_mul, TS(8, 0), TS(2, 0), TS(16, 0));

    /*
     * Division
     */
    TEST(fp_div, TS(0, 0), TS(1, 0), TS(0, 0));
    TEST(fp_div, TS(0, 0), TS(0, 1), TS(0, 0));
    TEST(fp_div, TS(0, 0), TS(1, 1), TS(0, 0));
    TEST(fp_div, TS(1, 0), TS(1, 0), TS(1, 0));
    TEST(fp_div, TS(1, 1), TS(1, 0), TS(1, 1));
    TEST(fp_div, TS(0, 1), TS(1, 0), TS(0, 1));
    TEST(fp_div, TS(0, TLOG_TIMESPEC_NSEC_PER_SEC - 1), TS(1, 0), TS(0, TLOG_TIMESPEC_NSEC_PER_SEC -1 ));
    TEST(fp_div, TS(0, 1), TS(0, 500000000), TS(0, 2));

    TEST(fp_div, TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(1, 0));
    TEST(fp_div, TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-1, 0));
    TEST(fp_div, TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-1, 0));
    TEST(fp_div, TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(1, 0));

    TEST(fp_div, TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(1, 0), TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0));
    TEST(fp_div, TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(1, 0), TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0));
    TEST(fp_div, TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-1, 0), TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0));
    TEST(fp_div, TS(-TLOG_TIMESPEC_NSEC_PER_SEC, 0), TS(-1, 0), TS(TLOG_TIMESPEC_NSEC_PER_SEC, 0));

    TEST(fp_div, tlog_timespec_max, tlog_timespec_max, TS(1, 0));
    TEST(fp_div, tlog_timespec_min, tlog_timespec_min, TS(1, 0));
    TEST(fp_div, tlog_timespec_max, TS(LONG_MIN+1, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), TS(-1, 0));
    TEST(fp_div, TS(LONG_MIN+1, -TLOG_TIMESPEC_NSEC_PER_SEC + 1), tlog_timespec_max, TS(-1, 0));

    TEST(fp_div, tlog_timespec_max, TS(1, 0), max_fp);
    TEST(fp_div, tlog_timespec_min, TS(1, 0), min_fp);

    /* Time scaling values */
    TEST(fp_div, TS(16, 0), TS(2, 0), TS(8, 0));
    TEST(fp_div, TS(8, 0), TS(2, 0), TS(4, 0));
    TEST(fp_div, TS(4, 0), TS(2, 0), TS(2, 0));
    TEST(fp_div, TS(2, 0), TS(2, 0), TS(1, 0));
    TEST(fp_div, TS(1, 0), TS(2, 0), TS(0, 500000000));
    TEST(fp_div, TS(0, 500000000), TS(2, 0), TS(0, 250000000));
    TEST(fp_div, TS(0, 250000000), TS(2, 0), TS(0, 125000000));
    TEST(fp_div, TS(0, 125000000), TS(2, 0), TS(0, 62500000));

    return !passed;
}
