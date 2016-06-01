/**
 * @file
 * @brief JSON terminal data sink.
 *
 * JSON terminal data sink accepts packets, formats log messages and sends
 * them to the writer specified upon creation.
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

#ifndef _TLOG_JSON_SINK_H
#define _TLOG_JSON_SINK_H

#include <tlog/sink.h>
#include <tlog/json_chunk.h>
#include <tlog/json_writer.h>

/** Minimum value of data chunk size */
#define TLOG_JSON_SINK_CHUNK_SIZE_MIN   TLOG_JSON_CHUNK_SIZE_MIN

/** JSON sink type */
extern const struct tlog_sink_type tlog_json_sink_type;

/**
 * Create (allocate and initialize) a JSON log sink.
 *
 * @param psink             Location for created sink pointer, set to NULL in
 *                          case of error.
 * @param writer            JSON log message writer.
 * @param writer_owned      True if the writer should be destroyed upon
 *                          destruction of the sink, false otherwise.
 * @param hostname          Hostname to use in log messages,
 *                          must be valid UTF-8.
 * @param username          Username to use in log messages,
 *                          must be valid UTF-8.
 * @param terminal          Terminal type string to use in log messages,
 *                          must be valid UTF-8.
 * @param session_id        Session ID to use in log messages.
 * @param chunk_size        Maximum data chunk length.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_json_sink_create(struct tlog_sink **psink,
                      struct tlog_json_writer *writer,
                      bool writer_owned,
                      const char *hostname,
                      const char *username,
                      const char *terminal,
                      unsigned int session_id,
                      size_t chunk_size)
{
    assert(psink != NULL);
    assert(tlog_json_writer_is_valid(writer));
    assert(hostname != NULL);
    assert(username != NULL);
    assert(terminal != NULL);
    assert(session_id != 0);
    assert(chunk_size >= TLOG_JSON_SINK_CHUNK_SIZE_MIN);

    return tlog_sink_create(psink, &tlog_json_sink_type,
                            writer, writer_owned,
                            hostname, username, terminal,
                            session_id, chunk_size);
}

#endif /* _TLOG_JSON_SINK_H */
