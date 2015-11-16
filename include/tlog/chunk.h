/*
 * Tlog JSON encoder data chunk buffer.
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

#ifndef _TLOG_CHUNK_H
#define _TLOG_CHUNK_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <tlog/rc.h>
#include <tlog/trx.h>
#include <tlog/misc.h>
#include <tlog/stream.h>

/** Minimum value of chunk size */
#define TLOG_CHUNK_SIZE_MIN    TLOG_STREAM_SIZE_MIN

/** Chunk buffer */
struct tlog_chunk {
    size_t              size;       /**< Maximum total data length and
                                         size of each buffer below */
    size_t              rem;        /**< Remaining total buffer space */

    uint8_t            *timing_buf; /**< Timing buffer */
    uint8_t            *timing_ptr; /**< Timing output pointer */

    struct tlog_stream  input;      /**< Input stream state and buffer */
    struct tlog_stream  output;     /**< Output stream state and buffer */

    struct timespec     first;      /**< First write timestamp */
    struct timespec     last;       /**< Last write timestamp */
};

/** Chunk transaction store */
struct tlog_chunk_trx_store {
    size_t              rem;        /**< Remaining total buffer space */
    uint8_t            *timing_ptr; /**< Timing output pointer */
    struct timespec     first;      /**< First write timestamp */
    struct timespec     last;       /**< Last write timestamp */

    struct tlog_stream_trx_store    input;  /**< Input store */
    struct tlog_stream_trx_store    output; /**< Output store */
};

/**
 * Make a transaction backup of a chunk.
 *
 * @param store     Transaction store to backup to.
 * @param object    Chunk object to backup.
 */
static inline void
tlog_chunk_trx_backup(struct tlog_chunk_trx_store *store,
                      struct tlog_chunk *object)
{
    store->rem          = object->rem;
    store->timing_ptr   = object->timing_ptr;
    store->first        = object->first;
    store->last         = object->last;
    tlog_stream_trx_backup(&store->input, &object->input);
    tlog_stream_trx_backup(&store->output, &object->output);
}

/**
 * Restore a chunk from a transaction backup.
 *
 * @param store     Transaction store to restore from.
 * @param object    Chunk object to restore.
 */
static inline void
tlog_chunk_trx_restore(struct tlog_chunk_trx_store *store,
                       struct tlog_chunk *object)
{
    object->rem          = store->rem;
    object->timing_ptr   = store->timing_ptr;
    object->first        = store->first;
    object->last         = store->last;
    tlog_stream_trx_restore(&store->input, &object->input);
    tlog_stream_trx_restore(&store->output, &object->output);
}

/**
 * Initialize a chunk.
 *
 * @param chunk The chunk to initialize.
 * @param size  Size of chunk.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_chunk_init(struct tlog_chunk *chunk, size_t size);

/**
 * Check if a chunk is valid.
 *
 * @param chunk The chunk to check.
 *
 * @return True if the chunk is valid, false otherwise.
 */
extern bool tlog_chunk_is_valid(const struct tlog_chunk *chunk);

/**
 * Check if a chunk has any incomplete characters pending.
 *
 * @param chunk The chunk to check.
 *
 * @return True if the chunk has incomplete characters pending, false
 *         otherwise.
 */
extern bool tlog_chunk_is_pending(const struct tlog_chunk *chunk);

/**
 * Check if a chunk is empty (no data in buffers, except the possibly
 * pending incomplete characters).
 *
 * @param chunk     The chunk to check.
 *
 * @return True if the chunk is empty, false otherwise.
 */
extern bool tlog_chunk_is_empty(const struct tlog_chunk *chunk);

/**
 * Write I/O to a chunk.
 *
 * @param chunk     The chunk to write to.
 * @param timestamp Input arrival timestamp.
 * @param output    True if writing output, false if input.
 * @param pbuf      Location of/for input buffer pointer.
 * @param plen      Location of/for input buffer length.
 *
 * @return Number of input bytes written.
 */
extern size_t tlog_chunk_write(struct tlog_chunk *chunk,
                               const struct timespec *timestamp,
                               bool output,
                               const uint8_t **pbuf,
                               size_t *plen);

/**
 * Flush a chunk - write metadata records to reserved space and reset
 * runs.
 *
 * @param chunk    The chunk to flush.
 */
extern void tlog_chunk_flush(struct tlog_chunk *chunk);

/**
 * Cut a chunk - write pending incomplete characters to the buffers.
 *
 * @param chunk     The chunk to cut.
 *
 * @return True if all incomplete characters fit into the remaining space,
 *         false otherwise.
 */
extern bool tlog_chunk_cut(struct tlog_chunk *chunk);

/**
 * Empty a chunk contents (but not pending incomplete characters).
 *
 * @param chunk     The chunk to empty.
 */
extern void tlog_chunk_empty(struct tlog_chunk *chunk);

/**
 * Cleanup a chunk (free any allocated data).
 *
 * @param chunk     The chunk to cleanup.
 */
extern void tlog_chunk_cleanup(struct tlog_chunk *chunk);

#endif /* _TLOG_CHUNK_H */
