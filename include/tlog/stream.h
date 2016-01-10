/**
 * @file
 * @brief Terminal I/O stream.
 *
 * A stream encodes one terminal I/O stream (input or output) into two forks:
 * text and binary JSON representation, which can then be used to format a
 * message. It takes a generic dispatcher upon creation which is used to
 * update the chunk's last timestamp, request space from the common budget and
 * write to timing metadata.
 */
/*
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

#include <tlog/grc.h>
#include <tlog/utf8.h>
#include <tlog/trx.h>
#include <tlog/dispatcher.h>

/** Minimum stream's text/binary buffer size */
#define TLOG_STREAM_SIZE_MIN    32

/** Stream transaction store */
TLOG_TRX_BASIC_STORE_SIG(tlog_stream) {
    struct tlog_utf8    utf8;           /**< UTF-8 filter */
    struct timespec     ts;             /**< Character end timestamp */
    size_t              txt_run;        /**< Text input run in characters */
    size_t              txt_dig;        /**< Text output run digit limit */
    size_t              txt_len;        /**< Text output length in bytes */
    size_t              bin_run;        /**< Binary input run in bytes */
    size_t              bin_dig;        /**< Binary output run digit limit */
    size_t              bin_len;        /**< Binary output length in bytes */
};

/** I/O stream */
struct tlog_stream {
    struct tlog_dispatcher *dispatcher; /**< Dispatcher to use */

    size_t              size;           /**< Text/binary encoded buffer size */

    struct tlog_utf8    utf8;           /**< UTF-8 filter */
    struct timespec     ts;             /**< Character end timestamp */

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

    struct tlog_trx_iface   trx_iface;      /**< Transaction interface */
    TLOG_TRX_BASIC_MEMBERS(tlog_stream);    /**< Transaction data */
};

/**
 * Initialize a stream.
 *
 * @param stream        The stream to initialize.
 * @param dispatcher    The dispatcher to use.
 * @param size          Text/binary buffer size.
 * @param valid_mark    Valid UTF-8 record marker character.
 * @param invalid_mark  Invalid UTF-8 record marker character.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_stream_init(struct tlog_stream *stream,
                                 struct tlog_dispatcher *dispatcher,
                                 size_t size,
                                 uint8_t valid_mark,
                                 uint8_t invalid_mark);

/**
 * Check if a stream is valid.
 *
 * @param stream    The stream to check.
 *
 * @return True if the stream is valid, false otherwise.
 */
extern bool tlog_stream_is_valid(const struct tlog_stream *stream);

/**
 * Check if a stream has an incomplete character pending.
 *
 * @param stream    The stream to check.
 *
 * @return True if the stream has an incomplete character pending, false
 *         otherwise.
 */
extern bool tlog_stream_is_pending(const struct tlog_stream *stream);

/**
 * Check if a stream is empty (no data in buffers, except the possibly
 * pending incomplete character).
 *
 * @param stream    The stream to check.
 *
 * @return True if the stream is empty, false otherwise.
 */
extern bool tlog_stream_is_empty(const struct tlog_stream *stream);

/**
 * Write to a stream, accounting for any (potentially) used total remaining
 * space.
 *
 * @param trx       Transaction to act within.
 * @param stream    The stream to write to.
 * @param ts        The write timestamp.
 * @param pbuf      Location of/for the pointer to the data to write.
 * @param plen      Location of/for the length of the data to write.
 *
 * @return Number of input bytes written.
 */
extern size_t tlog_stream_write(tlog_trx_state trx,
                                struct tlog_stream *stream,
                                const struct timespec *ts,
                                const uint8_t **pbuf, size_t *plen);

/**
 * Flush a stream - write metadata record to reserved space and reset runs.
 *
 * @param stream    The stream to flush.
 */
extern void tlog_stream_flush(struct tlog_stream *stream);

/**
 * Cut a stream - write pending incomplete character to the buffers.
 *
 * @param trx       Transaction to act within.
 * @param stream    The stream to cut.
 *
 * @return True if incomplete character fit into the remaining space, false
 *         otherwise.
 */
extern bool tlog_stream_cut(tlog_trx_state trx,
                            struct tlog_stream *stream);

/**
 * Empty buffers of a stream, but not pending incomplete characters.
 *
 * @param stream    The stream to empty.
 */
extern void tlog_stream_empty(struct tlog_stream *stream);

/**
 * Cleanup a stream. Can be called repeatedly.
 *
 * @param stream    The stream to cleanup.
 */
extern void tlog_stream_cleanup(struct tlog_stream *stream);

/**
 * Print a byte to a buffer in decimal, as much as it fits.
 *
 * @param buf   The buffer pointer.
 * @param len   The buffer length.
 * @param b     The byte to print.
 *
 * @return Length of the number (to be) printed.
 */
extern size_t tlog_stream_btoa(uint8_t *buf, size_t len, uint8_t b);

/**
 * Encode a valid UTF-8 byte sequence into a JSON string buffer atomically.
 * Reserve space for adding input length in decimal characters.
 *
 * @param trx           Transaction to act within.
 * @param dispatcher    The dispatcher to reserve space from.
 * @param obuf          Output buffer.
 * @param polen         Location of/for output byte counter.
 * @param pirun         Location of/for input character counter.
 * @param pidig         Location of/for the next digit input counter limit.
 * @param ibuf          Input buffer.
 * @param ilen          Input length.
 *
 * @return True if both the character and the new counter did fit into the
 *         remaining output space.
 */
extern bool tlog_stream_enc_txt(tlog_trx_state trx,
                                struct tlog_dispatcher *dispatcher,
                                uint8_t *obuf, size_t *polen,
                                size_t *pirun, size_t *pidig,
                                const uint8_t *ibuf, size_t ilen);

/**
 * Encode an invalid UTF-8 byte sequence into a JSON array buffer atomically.
 * Reserve space for adding input length in decimal bytes.
 *
 * @param trx           Transaction to act within.
 * @param dispatcher    The dispatcher to reserve space from.
 * @param obuf          Output buffer.
 * @param polen         Location of/for output byte counter.
 * @param pirun         Location of/for input character counter.
 * @param pidig         Location of/for the next digit input counter limit.
 * @param ibuf          Input buffer.
 * @param ilen          Input length.
 *
 * @return True if both the bytes and the new counter did fit into the
 *         remaining output space.
 */
extern bool tlog_stream_enc_bin(tlog_trx_state trx,
                                struct tlog_dispatcher *dispatcher,
                                uint8_t *obuf, size_t *polen,
                                size_t *pirun, size_t *pidig,
                                const uint8_t *ibuf, size_t ilen);

#endif /* _TLOG_STREAM_H */
