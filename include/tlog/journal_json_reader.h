/**
 * @file
 * @brief Systemd journal JSON message reader.
 *
 * An implementation of a JSON message reader retrieving log messages from
 * systemd journal.
 */
/*
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

#ifndef _TLOG_JOURNAL_JSON_READER_H
#define _TLOG_JOURNAL_JSON_READER_H

#include <assert.h>
#include <tlog/json_reader.h>
#include <tlog/journal_misc.h>

/** Systemd journal message reader type */
extern const struct tlog_json_reader_type tlog_journal_json_reader_type;

/**
 * Create a systemd journal reader.
 *
 * @param preader           Location for the created reader pointer, will be
 *                          set to NULL in case of error.
 * @param since             The realtime timestamp to seek to before
 *                          reading entries, in microseconds since epoch.
 * @param until             The realtime timestamp to read entries until,
 *                          in microseconds since epoch.
 * @param match_sym_list    NULL-terminated array of string pointers,
 *                          containing journal matche symbols to filter entries with.
 *                          See sd_journal_add_match(3). NULL means empty.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_journal_json_reader_create(struct tlog_json_reader **preader,
                                uint64_t since, uint64_t until,
                                const char * const *match_sym_list)
{
    assert(preader != NULL);
    assert(match_sym_list == NULL ||
           tlog_journal_match_sym_list_is_valid(match_sym_list));
    return tlog_json_reader_create(preader, &tlog_journal_json_reader_type,
                                   since, until, match_sym_list);
}

#endif /* _TLOG_JOURNAL_JSON_READER_H */
