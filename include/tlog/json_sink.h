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

/** JSON sink creation parameters */
struct tlog_json_sink_params {
    /** JSON log message writer */
    struct tlog_json_writer    *writer;
    /**
     * True if the writer should be destroyed upon destruction of the sink,
     * false otherwise
     */
    bool                        writer_owned;
    /** Hostname to use in log messages, must be valid UTF-8 */
    const char                 *hostname;
    /** Username to use in log messages, must be valid UTF-8 */
    const char                 *username;
    /** Terminal type string to use in log messages, must be valid UTF-8 */
    const char                 *terminal;
    /** Session ID to use in log messages */
    unsigned int                session_id;
    /** Maximum data chunk length */
    size_t                      chunk_size;
};

/**
 * Check if JSON sink creation parameters structure is valid.
 *
 * @param params    The parameters structure to check.
 *
 * @return True if the parameters structure is valid, false otherwise.
 */
extern bool tlog_json_sink_params_is_valid(
                            const struct tlog_json_sink_params *params);

/** JSON sink type */
extern const struct tlog_sink_type tlog_json_sink_type;

/**
 * Create (allocate and initialize) a JSON log sink.
 *
 * @param psink     Location for created sink pointer,
 *                  set to NULL in case of error.
 * @param params    Creation parameters structure.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_json_sink_create(struct tlog_sink **psink,
                      const struct tlog_json_sink_params *params)
{
    assert(psink != NULL);
    assert(tlog_json_sink_params_is_valid(params));

    return tlog_sink_create(psink, &tlog_json_sink_type, params);
}

#endif /* _TLOG_JSON_SINK_H */
