/**
 * @file
 * @brief Systemd journal JSON message writer
 *
 * An implementation of a JSON log message writer which sends messages to
 * systemd's journal service.
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

#ifndef _TLOG_JOURNAL_JSON_WRITER_H
#define _TLOG_JOURNAL_JSON_WRITER_H

#include <tlog/json_writer.h>
#include <tlog/syslog_misc.h>
#include <assert.h>

/**
 * Syslog message writer type
 *
 * Creation arguments:
 *
 * int  priority    The "priority" argument to pass to journal(3).
 */
extern const struct tlog_json_writer_type tlog_journal_json_writer_type;

/**
 * Create an instance of journal writer.
 *
 * @param pwriter   Location for the created writer pointer, will be set to
 *                  NULL in case of error.
 * @param priority  The "priority" argument to pass to journal(3).
 * @param username  Name of the user being recorded.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_journal_json_writer_create(struct tlog_json_writer **pwriter,
                                int priority,
                                const char *username,
                                unsigned int session_id)
{
    assert(pwriter != NULL);
    assert(tlog_syslog_priority_is_valid(priority));
    assert(username != NULL);
    assert(session_id != 0);
    return tlog_json_writer_create(pwriter, &tlog_journal_json_writer_type,
                                   priority, username, session_id);
}

#endif /* _TLOG_JOURNAL_JSON_WRITER_H */
