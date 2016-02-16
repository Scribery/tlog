/**
 * @file
 * @brief JSON terminal data source.
 *
 * JSON terminal data source provides an interface to read terminal data
 * packets, which are parsed from log messages retrieved from a JSON log
 * message reader specified upon creation.
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

#ifndef _TLOG_JSON_SOURCE_H
#define _TLOG_JSON_SOURCE_H

#include <tlog/json_reader.h>
#include <tlog/source.h>
#include <tlog/json_msg.h>

/** Minimum length of I/O data buffer used in packets */
#define TLOG_JSON_SOURCE_IO_SIZE_MIN TLOG_JSON_MSG_IO_SIZE_MIN

/** JSON source type */
extern const struct tlog_source_type tlog_json_source_type;

/**
 * Create (allocate and initialize) a log source.
 *
 * @param psource           Location for created source pointer, set to NULL
 *                          in case of error.
 * @param reader            Log message reader.
 * @param reader_owned      True if the reader should be destroyed upon
 *                          destruction of the sink, false otherwise.
 * @param hostname          Hostname to filter log messages by, NULL for
 *                          unfiltered.
 * @param username          Username to filter log messages by, NULL for
 *                          unfiltered.
 * @param session_id        Session ID to filter log messages by, 0 for
 *                          unfiltered.
 * @param io_size           Size of I/O data buffer used in packets.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_json_source_create(struct tlog_source **psource,
                        struct tlog_json_reader *reader,
                        bool reader_owned,
                        const char *hostname,
                        const char *username,
                        unsigned int session_id,
                        size_t io_size)
{
    assert(psource != NULL);
    assert(tlog_json_reader_is_valid(reader));
    assert(io_size >= TLOG_JSON_SOURCE_IO_SIZE_MIN);

    return tlog_source_create(psource, &tlog_json_source_type,
                              reader, reader_owned,
                              hostname, username, session_id, io_size);
}

#endif /* _TLOG_JSON_SOURCE_H */
