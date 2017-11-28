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

/* NOTE: Not using the macro from the header to workaround a gcc 4.8 bug */
const struct timespec tlog_timespec_zero = {0, 0};

const struct timespec tlog_timespec_min = {LONG_MIN, -TLOG_TIMESPEC_NSEC_PER_SEC + 1};

const struct timespec tlog_timespec_max = {LONG_MAX, TLOG_TIMESPEC_NSEC_PER_SEC - 1};

#define TLOG_TIMESPEC_FP_OP_ADD +
#define TLOG_TIMESPEC_FP_OP_SUB -
#define TLOG_TIMESPEC_FP_OP_MUL *
#define TLOG_TIMESPEC_FP_OP_DIV /

#define TLOG_TIMESPEC_FP_CALC(_a, _op, _b, _res) \
    do {                                            \
        double _ts;                                 \
                                                    \
        _ts = tlog_timespec_to_fp(a)                \
              TLOG_TIMESPEC_FP_OP_##_op             \
              tlog_timespec_to_fp(b);               \
        _ts = tlog_timespec_fp_cap(_ts);            \
        tlog_timespec_from_fp(_ts, _res);           \
    } while (0)

void
tlog_timespec_add(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    struct timespec tmp;

    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    tmp.tv_sec = a->tv_sec + b->tv_sec;
    tmp.tv_nsec = a->tv_nsec + b->tv_nsec;

    /* Carry from nsec */
    if (b->tv_sec >= 0 && b->tv_nsec >= 0) {
        if (tmp.tv_sec >= 0 ? tmp.tv_nsec >= TLOG_TIMESPEC_NSEC_PER_SEC
                            : tmp.tv_nsec > 0) {
            tmp.tv_sec++;
            tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    } else {
        if (tmp.tv_sec > 0 ? tmp.tv_nsec < 0
                           : tmp.tv_nsec <= -TLOG_TIMESPEC_NSEC_PER_SEC) {
            tmp.tv_sec--;
            tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    }

    /* Carry from sec */
    if (tmp.tv_sec < 0 && tmp.tv_nsec > 0) {
        tmp.tv_sec++;
        tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
    } else if (tmp.tv_sec > 0 && tmp.tv_nsec < 0) {
        tmp.tv_sec--;
        tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
    }

    *res = tmp;
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_sub(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *res)
{
    struct timespec tmp;

    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    tmp.tv_sec = a->tv_sec - b->tv_sec;
    tmp.tv_nsec = a->tv_nsec - b->tv_nsec;

    /* Carry from nsec */
    if (b->tv_sec < 0 || b->tv_nsec < 0) {
        if (tmp.tv_sec >= 0 ? tmp.tv_nsec >= TLOG_TIMESPEC_NSEC_PER_SEC
                            : tmp.tv_nsec > 0) {
            tmp.tv_sec++;
            tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    } else {
        if (tmp.tv_sec > 0 ? tmp.tv_nsec < 0
                           : tmp.tv_nsec <= -TLOG_TIMESPEC_NSEC_PER_SEC) {
            tmp.tv_sec--;
            tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    }

    /* Carry from sec */
    if (tmp.tv_sec < 0 && tmp.tv_nsec > 0) {
        tmp.tv_sec++;
        tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
    } else if (tmp.tv_sec > 0 && tmp.tv_nsec < 0) {
        tmp.tv_sec--;
        tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
    }

    *res = tmp;
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_cap_add(const struct timespec *a,
                      const struct timespec *b,
                      struct timespec *res)
{
    struct timespec tmp;

    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    tmp.tv_sec = a->tv_sec + b->tv_sec;

    /* If overflow */
    if ((a->tv_sec >= 0) == (b->tv_sec >= 0) &&
        (a->tv_sec >= 0) != (tmp.tv_sec >= 0)) {
        *res = (tmp.tv_sec >= 0) ? tlog_timespec_min : tlog_timespec_max;
        goto exit;
    }

    tmp.tv_nsec = a->tv_nsec + b->tv_nsec;

    /* Carry from nsec */
    if (b->tv_sec >= 0 && b->tv_nsec >= 0) {
        if (tmp.tv_sec >= 0 ? tmp.tv_nsec >= TLOG_TIMESPEC_NSEC_PER_SEC
                            : tmp.tv_nsec > 0) {
            /* If overflow */
            if (tmp.tv_sec == LONG_MAX) {
                *res = tlog_timespec_max;
                goto exit;
            }
            tmp.tv_sec++;
            tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    } else {
        if (tmp.tv_sec > 0 ? tmp.tv_nsec < 0
                           : tmp.tv_nsec <= -TLOG_TIMESPEC_NSEC_PER_SEC) {
            /* If overflow */
            if (tmp.tv_sec == LONG_MIN) {
                *res = tlog_timespec_min;
                goto exit;
            }
            tmp.tv_sec--;
            tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    }

    /* Carry from sec */
    if (tmp.tv_sec < 0 && tmp.tv_nsec > 0) {
        tmp.tv_sec++;
        tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
    } else if (tmp.tv_sec > 0 && tmp.tv_nsec < 0) {
        tmp.tv_sec--;
        tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
    }

    *res = tmp;
exit:
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_cap_sub(const struct timespec *a,
                      const struct timespec *b,
                      struct timespec *res)
{
    struct timespec tmp;

    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);

    tmp.tv_sec = a->tv_sec - b->tv_sec;

    /* If overflow */
    if ((a->tv_sec >= 0) != (b->tv_sec >= 0) &&
        (a->tv_sec >= 0) != (tmp.tv_sec >= 0)) {
        *res = (tmp.tv_sec >= 0) ? tlog_timespec_min : tlog_timespec_max;
        goto exit;
    }

    tmp.tv_nsec = a->tv_nsec - b->tv_nsec;

    /* Carry from nsec */
    if (b->tv_sec < 0 || b->tv_nsec < 0) {
        if (tmp.tv_sec >= 0 ? tmp.tv_nsec >= TLOG_TIMESPEC_NSEC_PER_SEC
                            : tmp.tv_nsec > 0) {
            /* If overflow */
            if (tmp.tv_sec == LONG_MAX) {
                *res = tlog_timespec_max;
                goto exit;
            }
            tmp.tv_sec++;
            tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    } else {
        if (tmp.tv_sec > 0 ? tmp.tv_nsec < 0
                           : tmp.tv_nsec <= -TLOG_TIMESPEC_NSEC_PER_SEC) {
            /* If overflow */
            if (tmp.tv_sec == LONG_MIN) {
                *res = tlog_timespec_min;
                goto exit;
            }
            tmp.tv_sec--;
            tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
        }
    }

    /* Carry from sec */
    if (tmp.tv_sec < 0 && tmp.tv_nsec > 0) {
        tmp.tv_sec++;
        tmp.tv_nsec -= TLOG_TIMESPEC_NSEC_PER_SEC;
    } else if (tmp.tv_sec > 0 && tmp.tv_nsec < 0) {
        tmp.tv_sec--;
        tmp.tv_nsec += TLOG_TIMESPEC_NSEC_PER_SEC;
    }

    *res = tmp;
exit:
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_fp_add(const struct timespec *a,
                     const struct timespec *b,
                     struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);
    TLOG_TIMESPEC_FP_CALC(a, ADD, b, res);
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_fp_sub(const struct timespec *a,
                     const struct timespec *b,
                     struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);
    TLOG_TIMESPEC_FP_CALC(a, SUB, b, res);
    assert(tlog_timespec_is_valid(res));
}

void
tlog_timespec_fp_mul(const struct timespec *a,
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
tlog_timespec_fp_div(const struct timespec *a,
                     const struct timespec *b,
                     struct timespec *res)
{
    assert(tlog_timespec_is_valid(a));
    assert(tlog_timespec_is_valid(b));
    assert(res != NULL);
    TLOG_TIMESPEC_FP_CALC(a, DIV, b, res);
    assert(tlog_timespec_is_valid(res));
}
