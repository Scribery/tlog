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

/** JSON source creation parameters */
struct tlog_json_source_params {
    /** Log message reader */
    struct tlog_json_reader    *reader;
    /**
     * True if the reader should be destroyed upon destruction of the sink,
     * false otherwise
     */
    bool                        reader_owned;
    /** Hostname to filter log messages by, NULL for unfiltered */
    const char                 *hostname;
    /**
     * True if messages should be filtered by recording ID,
     * false otherwise
     */
    bool                        filter_recording;
    /**
     * Recording ID to filter log messages by,
     * NULL for missing field in v1 format
     */
    const char                 *recording;
    /** Username to filter log messages by, NULL for unfiltered */
    const char                 *username;
    /** Terminal type string to require in log messages, NULL for any */
    const char                 *terminal;
    /** Session ID to filter log messages by, 0 for unfiltered */
    unsigned int                session_id;
    /**
     * Ignore missing messages, i.e. message ID jumps, if true,
     * return TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER, if a missing
     * message is detected, if false.
     */
    bool                        lax;
    /** Size of I/O data buffer used in packets */
    size_t                      io_size;
};

/**
 * Check if JSON source creation parameters structure is valid.
 *
 * @param params    The parameters structure to check.
 *
 * @return True if the parameters structure is valid, false otherwise.
 */
extern bool tlog_json_source_params_is_valid(
                            const struct tlog_json_source_params *params);

/**
 * Create (allocate and initialize) a JSON source.
 *
 * @param psource   Location for created source pointer,
 *                  set to NULL in case of error.
 * @param params    Creation parameters structure.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_json_source_create(struct tlog_source **psource,
                        const struct tlog_json_source_params *params)
{
    assert(psource != NULL);
    assert(tlog_json_source_params_is_valid(params));

    return tlog_source_create(psource, &tlog_json_source_type, params);
}

#endif /* _TLOG_JSON_SOURCE_H */
