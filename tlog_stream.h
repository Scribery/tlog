/*
 * Tlog I/O stream.
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

#ifndef _TLOG_STREAM_H
#define _TLOG_STREAM_H

#include "tlog_rc.h"
#include "tlog_utf8.h"

#define TLOG_STREAM_SIZE_MIN    64

/** I/O stream */
struct tlog_stream {
    size_t              size;           /**< Text/binary encoded buffer size */

    struct tlog_utf8    utf8;           /**< UTF-8 filter */

    uint8_t             valid_mark;     /**< Valid text record marker */
    uint8_t             invalid_mark;   /**< Invalid text record marker */

    uint8_t            *txt_buf;        /**< Encoded text buffer */
    size_t              txt_run;        /**< Text input run in characters */
    size_t              txt_dig;        /**< Text output run digit limit */
    size_t              txt_len;        /**< Text output length in bytes */

    uint8_t            *bin_buf;        /**< Encoded binary buffer */
    size_t              bin_run;        /**< Binary input run in bytes */
    size_t              bin_dig;        /**< Binary output run digit limit */
    size_t              bin_len;        /**< Binary output length in bytes */
};

/**
 * Initialize a stream.
 *
 * @param stream        The stream to initialize.
 * @param size          Text/binary buffer size.
 * @param valid_mark    Valid UTF-8 record marker character.
 * @param invalid_mark  Invalid UTF-8 record marker character.
 */
extern tlog_rc tlog_stream_init(struct tlog_stream *stream, size_t size,
                                uint8_t valid_mark, uint8_t invalid_mark);

/**
 * Check if a stream is valid.
 *
 * @param stream    The stream to check.
 *
 * @return True if the stream is valid, false otherwise.
 */
extern bool tlog_stream_is_valid(const struct tlog_stream *stream);

/**
 * Write to a stream, accounting for any (potentially) used total remaining
 * space.
 *
 * @param stream    The stream to write to.
 * @param pbuf      Location of/for the pointer to the data to write.
 * @param plen      Location of/for the length of the data to write.
 * @param pmeta     Location of/for the meta data output pointer.
 * @param prem      Location of/for the total remaining output space.
 */
extern void tlog_stream_write(struct tlog_stream *stream,
                              uint8_t **pbuf, size_t *plen,
                              uint8_t **pmeta,
                              size_t *prem);

/**
 * Cut a stream (flush metadata record to reserved space) on a character
 * boundary.
 *
 * @param stream    The stream to cut.
 * @param pmeta     Location of/for the metadata output pointer.
 */
extern void tlog_stream_cut(struct tlog_stream *stream,
                            uint8_t **pmeta);

/**
 * Flush a stream (flush all the data, including incomplete characters).
 *
 * @param stream    The stream to flush.
 * @param pmeta     Location of/for the meta data output pointer.
 * @param prem      Location of/for the total remaining output space.
 *
 * @return True if data to flush fit into the remaining space, false
 *         otherwise.
 */
extern bool tlog_stream_flush(struct tlog_stream *stream,
                              uint8_t **pmeta,
                              size_t *prem);

/**
 * Cleanup a stream. Can be called repeatedly.
 *
 * @param stream    The stream to cleanup.
 */
extern void tlog_stream_cleanup(struct tlog_stream *stream);

#endif /* _TLOG_STREAM_H */
