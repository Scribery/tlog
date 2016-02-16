/*
 * Miscellaneous syslog-related functions.
 *
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

#include <tlog/syslog_misc.h>
#include <stdlib.h>
#include <string.h>
#define SYSLOG_NAMES
#include <syslog.h>

static int
tlog_syslog_code_from_str(CODE *list, const char *str)
{
    CODE *item;

    for (item = list; item->c_name != NULL; item++) {
        if (strcmp(str, item->c_name) == 0) {
            return item->c_val;
        }
    }
    return -1;
}

int
tlog_syslog_facility_from_str(const char *str)
{
    return tlog_syslog_code_from_str(facilitynames, str);
}

int
tlog_syslog_priority_from_str(const char *str)
{
    return tlog_syslog_code_from_str(prioritynames, str);
}
