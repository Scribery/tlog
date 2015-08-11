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

/** Minimum source error code value */
#define TLOG_SOURCE_ERROR_MIN   0x20000000

enum tlog_source_error {
    /* Object with invalid schema encountered in the stream */
    TLOG_SOURCE_ERROR_INVALID_OBJECT = TLOG_SOURCE_ERROR_MIN,
};

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
 * @return Zero on success, negated errno value, source or reader-specific
 *         error code on failure.
 */
extern int tlog_source_init(struct tlog_source *source,
                            struct tlog_reader *reader,
                            const char *hostname,
                            const char *username,
                            unsigned int session_id,
                            size_t io_size);

/**
 * Create (allocate and initialize) a log source.
 *
 * @param psource           Location for created source pointer.
 * @param reader            Log message reader.
 * @param hostname          Hostname to filter log messages by, NULL for
 *                          unfiltered.
 * @param username          Username to filter log messages by, NULL for
 *                          unfiltered.
 * @param session_id        Session ID to filter log messages by, 0 for
 *                          unfiltered.
 * @param io_size           Size of I/O data buffer used in packets.
 *
 * @return Zero on success, negated errno value, source or reader-specific
 *         error code on failure.
 */
extern int tlog_source_create(struct tlog_source **psource,
                              struct tlog_reader *reader,
                              const char *hostname,
                              const char *username,
                              unsigned int session_id,
                              size_t io_size);

/**
 * Retrieve a source error code description.
 *
 * @param source    The source to retrieve error code description for.
 * @param rc        Error code to retrieve description for.
 *
 * @return Statically-allocated error code description.
 */
extern const char *tlog_source_strerror(struct tlog_source *source, int rc);

/**
 * Read a packet from the source.
 *
 * @param source    The source to read from.
 * @param pkt       The packet to write the received data into, must be void,
 *                  will be set to void on end-of-stream.
 *
 * @return Zero on success, negated errno value, source or reader-specific
 *         error code on failure.
 */
extern int tlog_source_read(struct tlog_source *source,
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
