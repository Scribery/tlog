/*
 * Tlog source.
 *
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

#ifndef _TLOG_SOURCE_H
#define _TLOG_SOURCE_H

#include "tlog/reader.h"
#include "tlog/msg.h"

/** Minimum length of I/O data buffer used in packets */
#define TLOG_SOURCE_IO_SIZE_MIN TLOG_MSG_IO_SIZE_MIN

/* Source instance */
struct tlog_source {
    struct tlog_reader *reader;         /**< Log message reader */
    char               *hostname;       /**< Hostname to filter messages by,
                                             NULL for unfiltered */
    char               *username;       /**< Username to filter messages by,
                                             NULL for unfiltered */
    unsigned int        session_id;     /**< Session ID to filter messages by,
                                             NULL for unfiltered */

    /* FIXME: add validation */
    size_t              last_id;        /**< Last message ID */
    struct timespec     last_pos;       /**< Last message timestamp */

    struct tlog_msg     msg;            /**< Message parsing state */

    uint8_t            *io_buf;         /**< I/O data buffer used in packets */
    size_t              io_size;        /**< I/O data buffer length */
};

/**
 * Check if a source is valid.
 *
 * @param source      The source to check.
 *
 * @return True if the source is valid, false otherwise.
 */
extern bool tlog_source_is_valid(const struct tlog_source *source);

/**
 * Initialize a log source.
 *
 * @param source            Pointer to the source to initialize.
 * @param reader            Log message reader.
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
extern tlog_grc tlog_source_init(struct tlog_source *source,
                                 struct tlog_reader *reader,
                                 const char *hostname,
                                 const char *username,
                                 unsigned int session_id,
                                 size_t io_size);

/**
 * Create (allocate and initialize) a log source.
 *
 * @param psource           Location for created source pointer, set to NULL
 *                          in case of error.
 * @param reader            Log message reader.
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
extern tlog_grc tlog_source_create(struct tlog_source **psource,
                                   struct tlog_reader *reader,
                                   const char *hostname,
                                   const char *username,
                                   unsigned int session_id,
                                   size_t io_size);

/**
 * Retrieve current opaque location of the source.
 *
 * @param source    The source to retrieve current location from.
 *
 * @return Opaque location value.
 */
extern size_t tlog_source_loc_get(const struct tlog_source *source);

/**
 * Format opaque location as a string.
 *
 * @param source    The source to format location for.
 * @param loc       Opaque location value to format.
 *
 * @return Dynamically-allocated string describing the location, or NULL in
 *         case of error (see errno).
 */
extern char *tlog_source_loc_fmt(const struct tlog_source *source,
                                 size_t loc);

/**
 * Read a packet from the source.
 *
 * @param source    The source to read from.
 * @param pkt       The packet to write the received data into, must be void,
 *                  will be void on end-of-stream.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_source_read(struct tlog_source *source,
                                 struct tlog_pkt *pkt);

/**
 * Cleanup a log source. Can be called more than once.
 *
 * @param source  Pointer to the source to cleanup.
 */
extern void tlog_source_cleanup(struct tlog_source *source);

/**
 * Destroy (cleanup and free) a log source.
 *
 * @param source  Pointer to the source to destroy, can be NULL.
 */
extern void tlog_source_destroy(struct tlog_source *source);

#endif /* _TLOG_SOURCE_H */
