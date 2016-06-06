/**
 * @file
 * @brief Supported time span (delay) constants
 *
 * These constants describe minimum and maximum time span values supported by
 * JSON messages.
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

#ifndef _TLOG_DELAY_H
#define _TLOG_DELAY_H

#include <time.h>
#include <stdint.h>

/** Minimum delay in seconds, as a number */
#define TLOG_DELAY_MIN_S_NUM    0
/** Minimum delay in seconds, as a string */
#define TLOG_DELAY_MIN_S_STR    ""
/** Minimum delay in milliseconds, as a number */
#define TLOG_DELAY_MIN_MS_NUM   1
/** Minimum delay in milliseconds, as a string */
#define TLOG_DELAY_MIN_MS_STR  "1"

/** Minimum delay's struct timespec tv_sec field value */
#define TLOG_DELAY_MIN_TIMESPEC_SEC \
    TLOG_DELAY_MIN_S_NUM

/** Minimum delay's struct timespec tv_nsec field value */
#define TLOG_DELAY_MIN_TIMESPEC_NSEC \
    (TLOG_DELAY_MIN_MS_NUM % 1000 * 1000000)

/** Minimum delay as a timespec initializer */
#define TLOG_DELAY_MIN_TIMESPEC \
    (struct timespec){                              \
        .tv_sec = TLOG_DELAY_MIN_TIMESPEC_SEC,      \
        .tv_nsec = TLOG_DELAY_MIN_TIMESPEC_NSEC     \
    }

/** Minimum delay as a timespec constant */
extern const struct timespec tlog_delay_min_timespec;

/** Maximum delay in seconds, as a number */
#define TLOG_DELAY_MAX_S_NUM    INT32_MAX
/** Maximum delay in seconds, as a string */
#define TLOG_DELAY_MAX_S_STR    "2147483647"
/** Maximum delay in milliseconds, as a number */
#define TLOG_DELAY_MAX_MS_NUM ((int64_t)TLOG_DELAY_MAX_S_NUM * 1000 + 999)
/** Maximum delay in milliseconds, as a string */
#define TLOG_DELAY_MAX_MS_STR  TLOG_DELAY_MAX_S_STR "999"

/** Maximum delay's struct timespec tv_sec field value */
#define TLOG_DELAY_MAX_TIMESPEC_SEC \
    TLOG_DELAY_MAX_S_NUM

/** Maximum delay's struct timespec tv_nsec field value */
#define TLOG_DELAY_MAX_TIMESPEC_NSEC \
    (TLOG_DELAY_MAX_MS_NUM % 1000 * 1000000)

/** Maximum delay as a timespec initializer */
#define TLOG_DELAY_MAX_TIMESPEC \
    (struct timespec){                              \
        .tv_sec = TLOG_DELAY_MAX_TIMESPEC_SEC,      \
        .tv_nsec = TLOG_DELAY_MAX_TIMESPEC_NSEC     \
    }

/** Maximum delay as a timespec constant */
extern const struct timespec tlog_delay_max_timespec;

#endif /* _TLOG_DELAY_H */
