/*
 * Supported time span (delay) constants.
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

#include <tlog/delay.h>

/* NOTE: Not using the macro from the header to workaround a gcc 4.8 bug */
const struct timespec tlog_delay_min_timespec = {
    .tv_sec = TLOG_DELAY_MIN_TIMESPEC_SEC,
    .tv_nsec = TLOG_DELAY_MIN_TIMESPEC_NSEC
};
/* NOTE: Not using the macro from the header to workaround a gcc 4.8 bug */
const struct timespec tlog_delay_max_timespec = {
    .tv_sec = TLOG_DELAY_MAX_TIMESPEC_SEC,
    .tv_nsec = TLOG_DELAY_MAX_TIMESPEC_NSEC
};
