/**
 * @file
 * @brief Functions handling struct timespec.
 */
/*
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

#ifndef _TLOG_TIMESPEC_H
#define _TLOG_TIMESPEC_H

#include <time.h>
#include <stdbool.h>
#include <assert.h>

/**
 * Check that a timespec is valid.
 *
 * @param ts    The timespec to check.
 *
 * @return True if the timespec is valid, false otherwise.
 */
static inline bool
tlog_timespec_is_valid(const struct timespec *ts)
{
    return ts != NULL &&
           ts->tv_nsec < 1000000000 &&
           ts->tv_nsec > -1000000000 &&
           ((ts->tv_sec < 0)
                ? (ts->tv_nsec <= 0)
                : (ts->tv_sec == 0 || ts->tv_nsec >= 0));
}

/**
 * Subtract timespec b from timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
static inline void
tlog_timespec_sub(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (res->tv_sec > 0
            ? res->tv_nsec < 0
            : res->tv_nsec <= -1000000000) {
        res->tv_sec--;
        res->tv_nsec += 1000000000;
    }

    assert(tlog_timespec_is_valid(res));
}

/**
 * Add timespec b to timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
static inline void
tlog_timespec_add(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    res->tv_sec = a->tv_sec + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (res->tv_sec >= 0
            ? res->tv_nsec >= 1000000000
            : res->tv_nsec > 0) {
        res->tv_sec++;
        res->tv_nsec -= 1000000000;
    }

    assert(tlog_timespec_is_valid(res));
}

/**
 * Multiply timespec a by timespec b and put result into res.
 *
 * @param a     Timespec structure to multiply.
 * @param b     Timespec structure to multiply by.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_mul(const struct timespec *a,
                              const struct timespec *b,
                              struct timespec *res);

/**
 * Divide timespec a by timespec b and put result into res.
 *
 * @param a     Timespec structure to divide.
 * @param b     Timespec structure to divide by.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_div(const struct timespec *a,
                              const struct timespec *b,
                              struct timespec *res);

/**
 * Check if a timespec is zero.
 *
 * @param t     Timespec to check.
 *
 * @return True if the timespec is zero, false otherwise.
 */
static inline bool
tlog_timespec_is_zero(const struct timespec *t)
{
    return t->tv_sec == 0 && t->tv_nsec == 0;
}

/** Zero timespec initializer */
#define TLOG_TIMESPEC_ZERO  ((struct timespec){0, 0})

/** Zero timespec constant */
extern const struct timespec tlog_timespec_zero;

/**
 * Compare two timespec's.
 *
 * @param a     First timespec to compare.
 * @param b     Second timespec to compare.
 *
 * @return Comparison result:
 *          -1  - a < b
 *           0  - a == b
 *           1  - a > b
 */
static inline int
tlog_timespec_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec < b->tv_sec) {
        return -1;
    } else if (a->tv_sec > b->tv_sec) {
        return 1;
    } if (a->tv_nsec < b->tv_nsec) {
        return -1;
    } else if (a->tv_nsec > b->tv_nsec) {
        return 1;
    }
    return 0;
}

#endif /* _TLOG_TIMESPEC_H */
