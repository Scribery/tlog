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

#include <tlog/misc.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>

/** Zero timespec initializer */
#define TLOG_TIMESPEC_ZERO  ((struct timespec){0, 0})

/** Zero timespec constant */
extern const struct timespec tlog_timespec_zero;

/** Minimum timespec constant */
extern const struct timespec tlog_timespec_min;

/** Maximum timespec constant */
extern const struct timespec tlog_timespec_max;

/** Maximum value a double-precision floating point timespec can be */
#define TLOG_TIMESPEC_FP_MAX \
            nextafter((double)LONG_MAX + (double)0.999999999, 0)

/** Minimum value a double-precision floating point timespec can be */
#define TLOG_TIMESPEC_FP_MIN \
            nextafter((double)LONG_MIN - (double)0.999999999, 0)

/** Nanoseconds per second */
#define TLOG_TIMESPEC_NSEC_PER_SEC  1000000000

/**
 * Cap a double-precision floating point to timespec range
 */
static inline double
tlog_timespec_fp_cap(double n)
{
    return TLOG_MAX(TLOG_MIN(n, TLOG_TIMESPEC_FP_MAX), TLOG_TIMESPEC_FP_MIN);
}

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
           ts->tv_nsec < TLOG_TIMESPEC_NSEC_PER_SEC  &&
           ts->tv_nsec > -TLOG_TIMESPEC_NSEC_PER_SEC &&
           ((ts->tv_sec < 0)
                ? (ts->tv_nsec <= 0)
                : (ts->tv_sec == 0 || ts->tv_nsec >= 0));
}

/**
 * Convert a double-precision floating point number of seconds to timespec.
 *
 * @param n     Number of seconds.
 * @param res   Location for the conversion result.
 */
static inline void
tlog_timespec_from_fp(double n, struct timespec *res)
{
    double i, f;

    assert(res != NULL);

    f = modf(n, &i);
    res->tv_sec = i;
    res->tv_nsec = lround(f * TLOG_TIMESPEC_NSEC_PER_SEC);

    assert(tlog_timespec_is_valid(res));
}

/**
 * Convert a timespec to double-precision floating point number of seconds.
 *
 * @param ts    The timespec to convert.
 *
 * @return The resulting number of seconds.
 */
static inline double
tlog_timespec_to_fp(const struct timespec *ts)
{
    assert(tlog_timespec_is_valid(ts));
    return (double)ts->tv_sec + (double)ts->tv_nsec / TLOG_TIMESPEC_NSEC_PER_SEC ;
}

/**
 * Add timespec b to timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_add(const struct timespec *a,
                              const struct timespec *b,
                              struct timespec *res);

/**
 * Subtract timespec b from timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_sub(const struct timespec *a,
                              const struct timespec *b,
                              struct timespec *res);

/**
 * Add timespec b to timespec a and put the result in res, capping overflows
 * to minimum/maximum.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_cap_add(const struct timespec *a,
                                  const struct timespec *b,
                                  struct timespec *res);

/**
 * Subtract timespec b from timespec a and put the result in res, capping
 * overflows to minimum/maximum.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_cap_sub(const struct timespec *a,
                                  const struct timespec *b,
                                  struct timespec *res);

/**
 * Add timespec b to timespec a and put the result in res, using
 * double-precision floating point arithmetic.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_fp_add(const struct timespec *a,
                                 const struct timespec *b,
                                 struct timespec *res);

/**
 * Subtract timespec b from timespec a and put the result in res, using
 * double-precision floating point arithmetic.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_fp_sub(const struct timespec *a,
                                 const struct timespec *b,
                                 struct timespec *res);

/**
 * Multiply timespec a by timespec b and put result into res, using
 * double-precision floating point arithmetic.
 *
 * @param a     Timespec structure to multiply.
 * @param b     Timespec structure to multiply by.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_fp_mul(const struct timespec *a,
                                 const struct timespec *b,
                                 struct timespec *res);

/**
 * Divide timespec a by timespec b and put result into res, using
 * double-precision floating point arithmetic.
 *
 * @param a     Timespec structure to divide.
 * @param b     Timespec structure to divide by.
 * @param res   Location for result, can be a or b as well.
 */
extern void tlog_timespec_fp_div(const struct timespec *a,
                                 const struct timespec *b,
                                 struct timespec *res);

/**
 * Check if a timespec is negative (not positive and not zero).
 *
 * @param t     Timespec to check.
 *
 * @return True if the timespec is negative, false otherwise.
 */
static inline bool
tlog_timespec_is_negative(const struct timespec *t)
{
    assert(tlog_timespec_is_valid(t));
    return t->tv_sec < 0 || t->tv_nsec < 0;
}

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

/**
 * Check if a timespec is positive, (not negative and not zero).
 *
 * @param t     Timespec to check.
 *
 * @return True if the timespec is positive, false otherwise.
 */
static inline bool
tlog_timespec_is_positive(const struct timespec *t)
{
    assert(tlog_timespec_is_valid(t));
    return (t->tv_sec > 0 && t->tv_nsec >= 0) ||
           (t->tv_sec >= 0 && t->tv_nsec > 0);
}

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

/** Timespec printf-style format string */
#define TLOG_TIMESPEC_FMT "%s%ld.%09ld"

/**
 * Extract the sign string argument for printf-style format, required for
 * formatting certain timespec values.
 *
 * @param ts    The timespec to extract the sign string from.
 *
 * @return The sign string.
 */
static inline const char *
tlog_timespec_arg_sign(const struct timespec *ts)
{
    assert(tlog_timespec_is_valid(ts));
    return (ts->tv_sec == 0 && ts->tv_nsec < 0) ? "-" : "";
}

/**
 * Extract the seconds argument for timespec printf-style format.
 *
 * @param ts    The timespec to extract the seconds from.
 *
 * @return The seconds argument.
 */
static inline long
tlog_timespec_arg_sec(const struct timespec *ts)
{
    assert(tlog_timespec_is_valid(ts));
    return ts->tv_sec;
}

/**
 * Extract the nanoseconds argument for timespec printf-style format.
 *
 * @param ts    The timespec to extract the nanoseconds from.
 *
 * @return The nanoseconds argument.
 */
static inline long
tlog_timespec_arg_nsec(const struct timespec *ts)
{
    assert(tlog_timespec_is_valid(ts));
    return ts->tv_nsec < 0 ? -ts->tv_nsec : ts->tv_nsec;
}

/**
 * Expand to timespec printf-style format arguments.
 *
 * @param _ts   A timespec pointer to extract arguments from.
 */
#define TLOG_TIMESPEC_ARG(_ts) \
    tlog_timespec_arg_sign(_ts),    \
    tlog_timespec_arg_sec(_ts),     \
    tlog_timespec_arg_nsec(_ts)

#endif /* _TLOG_TIMESPEC_H */
