/*
 * Functions handling struct timespec.
 *
 * Copyright (C) 2015 Red Hat
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
#include <tlog/misc.h>
#include <limits.h>
#include <math.h>

/* NOTE: Not using the macro from the header to workaround a gcc 4.8 bug */
const struct timespec tlog_timespec_zero = {0, 0};

#define TLOG_TIMESPEC_FP_MAX \
            nextafter((double)LONG_MAX + (double)0.999999999, 0)
#define TLOG_TIMESPEC_FP_MIN \
            nextafter((double)LONG_MIN - (double)0.999999999, 0)
#define TLOG_TIMESPEC_FP_CAP(_x) \
            TLOG_MAX(TLOG_MIN(_x, TLOG_TIMESPEC_FP_MAX), TLOG_TIMESPEC_FP_MIN)

#define TLOG_TIMESPEC_FP_OP_MUL *
#define TLOG_TIMESPEC_FP_OP_DIV /

#define TLOG_TIMESPEC_FP_CALC(_a, _op, _b, _res) \
    do {                                                                    \
        double _ts;                                                         \
        double _i;                                                          \
        double _f;                                                          \
                                                                            \
        _ts = ((double)_a->tv_sec + (double)_a->tv_nsec / 1000000000)       \
              TLOG_TIMESPEC_FP_OP_##_op                                     \
              ((double)_b->tv_sec + (double)_b->tv_nsec / 1000000000);      \
        _ts = TLOG_TIMESPEC_FP_CAP(_ts);                                    \
        _f = modf(_ts, &_i);                                                \
        _res->tv_sec = (long)_i;                                            \
        _res->tv_nsec = (long)(_f * 1000000000);                            \
    } while (0)


void
tlog_timespec_mul(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);
    TLOG_TIMESPEC_FP_CALC(a, MUL, b, res);
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_div(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);
    TLOG_TIMESPEC_FP_CALC(a, DIV, b, res);
    assert(tlog_timespec_is_valid(res));
}
