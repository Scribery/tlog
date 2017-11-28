/*
 * Miscellaneous systemd journal-related functions.
 *
 * Copyright (C) 2017 Red Hat
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

#include <tlog/journal_misc.h>
#include <string.h>
#include <assert.h>

bool
tlog_journal_match_is_valid(const void *data, size_t size)
{
    const char *str = data;
    size_t i;
    char c;

    if (data == NULL) {
        return false;
    }

    for (i = 0; size == 0 || i < size; i++) {
        c = str[i];
        if (c == '\0' && size == 0) {
            return false;
        } if (c == '=') {
            return (i > 0);
        } else if (c == '_') {
            if (i == 1 && str[0] == '_') {
                return false;
            }
        } else if ((c < 'A' || c > 'Z') && (c < '0' || c > '9')) {
            return false;
        }
    }

    return true;
}

bool
tlog_journal_match_sym_is_valid(const char *match_sym)
{
    return match_sym != NULL &&
           (tlog_journal_match_is_valid(match_sym, 0) ||
            strcasecmp(match_sym, "OR") == 0 ||
            strcasecmp(match_sym, "AND") == 0);
}

int
tlog_journal_add_match_sym(sd_journal *j, const char *match_sym)
{
    assert(j != NULL);
    assert(tlog_journal_match_sym_is_valid(match_sym));

    if (strcasecmp(match_sym, "AND") == 0) {
        return sd_journal_add_conjunction(j);
    } else if (strcasecmp(match_sym, "OR") == 0) {
        return sd_journal_add_disjunction(j);
    } else {
        return sd_journal_add_match(j, match_sym, 0);
    }
}

bool
tlog_journal_match_sym_list_is_valid(const char * const *match_sym_list)
{
    const char * const *p;

    if (match_sym_list == NULL) {
        return false;
    }

    for (p = match_sym_list; *p != NULL; p++) {
        if (!tlog_journal_match_sym_is_valid(*p)) {
            return false;
        }
    }

    return true;
}

int
tlog_journal_add_match_sym_list(sd_journal *j,
                                const char * const *match_sym_list)
{
    const char * const *p;
    int sd_rc;

    assert(j != NULL);
    assert(match_sym_list == NULL ||
           tlog_journal_match_sym_list_is_valid(match_sym_list));

    if (match_sym_list != NULL) {
        for (p = match_sym_list; *p != NULL; p++) {
            sd_rc = tlog_journal_add_match_sym(j, *p);
            if (sd_rc < 0) {
                return sd_rc;
            }
        }
    }

    return 0;
}
