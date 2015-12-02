/*
 * Miscellaneous functions.
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

#ifndef _TLOG_MISC_H
#define _TLOG_MISC_H

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

/** Return number of elements in an array */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/** Return maximum of two numbers */
#define TLOG_MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))

/** Return minimum of two numbers */
#define TLOG_MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

/** Return an offset of a type member */
#define TLOG_OFFSET_OF(_type, _member) ((size_t) &((_type *)0)->_member)

/**
 * Cast a member of a structure out to the containing structure.
 *
 * @param _ptr      The pointer to the member.
 * @param _type     The type of the container this is embedded in.
 * @param _member   The name of the member within the type.
 *
 */
#define TLOG_CONTAINER_OF(_ptr, _type, _member) \
    ({                                                              \
        const typeof(((_type *)0)->_member) *_mptr = (_ptr);        \
        (_type *)((char *)_mptr - TLOG_OFFSET_OF(_type, _member));  \
    })

/**
 * Subtract timespec b from timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result.
 */
static inline void
tlog_timespec_sub(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (res->tv_nsec < 0) {
        res->tv_sec--;
        res->tv_nsec += 1000000000;
    }
}

/**
 * Add timespec b to timespec a and put the result in res.
 *
 * @param a     Timespec structure to subtract from.
 * @param b     Timespec structure to subtract.
 * @param res   Location for result.
 */
static inline void
tlog_timespec_add(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    res->tv_sec = a->tv_sec + b->tv_sec;
    res->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (res->tv_nsec > 1000000000) {
        res->tv_sec++;
        res->tv_nsec -= 1000000000;
    }
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

/** Zero timespec initializer */
#define TLOG_TIMESPEC_ZERO  ((struct timespec){0, 0})

/**
 * Set a timespec value to zero.
 *
 * @param t     Timespec to zero.
 */
static inline void
tlog_timespec_zero(struct timespec *t)
{
    *t = TLOG_TIMESPEC_ZERO;
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
    if (a->tv_sec < b->tv_sec)
        return -1;
    else if (a->tv_sec > b->tv_sec)
        return 1;
    if (a->tv_nsec < b->tv_nsec)
        return -1;
    else if (a->tv_nsec > b->tv_nsec)
        return 1;
    return 0;
}

/**
 * Convert a nibble (4-bit number) to a hexadecimal digit.
 *
 * @param n     Nibble to convert.
 *
 * @return Hexadecimal digit.
 */
static inline uint8_t
tlog_nibble_digit(uint8_t n)
{
    return (n < 10) ? ('0' + n) : ('a' - 10 + n);
}

/**
 * Calculate number of decimal digits in a size_t number.
 *
 * @param n     The number to count digits for.
 *
 * @return Number of digits.
 */
static inline size_t
tlog_size_digits(size_t n)
{
    size_t d;
    for (d = 1; n > 9; d++, n /= 10);
    return d;
}

#endif /* _TLOG_MISC_H */
