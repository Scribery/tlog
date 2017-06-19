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
#include <string.h>
#define SYSLOG_NAMES
#include <syslog.h>

/**
 * Find a code value in a syslog code list and return its name.
 *
 * @param list  The syslog code list to search.
 * @param val   The code value to find.
 *
 * @return The code name, or NULL, if the code value was not found.
 */
static const char *
tlog_syslog_code_to_str(CODE *list, int val)
{
    CODE *item;

    for (item = list; item->c_name != NULL; item++) {
        if (item->c_val == val) {
            return item->c_name;
        }
    }
    return NULL;
}

/**
 * Find a code name in a syslog code list and return its value.
 *
 * @param list  The syslog code list to search.
 * @param name  The code name to find.
 *
 * @return The code value, or a negative number, if the name was not found in
 *         the list.
 */
static int
tlog_syslog_code_from_str(CODE *list, const char *name)
{
    CODE *item;

    for (item = list; item->c_name != NULL; item++) {
        if (strcasecmp(name, item->c_name) == 0) {
            return item->c_val;
        }
    }
    return -1;
}

const char *
tlog_syslog_facility_to_str(int val)
{
    return tlog_syslog_code_to_str(facilitynames, val);
}

int
tlog_syslog_facility_from_str(const char *str)
{
    return tlog_syslog_code_from_str(facilitynames, str);
}

const char *
tlog_syslog_priority_to_str(int val)
{
    return tlog_syslog_code_to_str(prioritynames, val);
}

int
tlog_syslog_priority_from_str(const char *str)
{
    return tlog_syslog_code_from_str(prioritynames, str);
}
