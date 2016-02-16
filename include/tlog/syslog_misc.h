/**
 * @file
 * @brief Miscellaneous syslog-related functions.
 *
 * A collection of miscellaneous syslog-related function.
 */
/*
 * Copyright (C) 2016 Red Hat
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

#ifndef _TLOG_SYSLOG_MISC_H
#define _TLOG_SYSLOG_MISC_H

/**
 * Convert syslog facility string to integer value.
 *
 * @param str   The string to convert.
 *
 * @return The facility value, which can be supplied to openlog(3) and
 *         syslog(3), or a negative number, if the string was unrecognized.
 */
extern int tlog_syslog_facility_from_str(const char *str);

/**
 * Convert syslog priority string to integer value.
 *
 * @param str   The string to convert.
 *
 * @return The priority value, which can be supplied to openlog(3) and
 *         syslog(3), or a negative number, if the string was unrecognized.
 */
extern int tlog_syslog_priority_from_str(const char *str);

#endif /* _TLOG_SYSLOG_MISC_H */
