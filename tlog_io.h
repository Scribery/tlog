/*
 * Tlog I/O buffer.
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

#ifndef _TLOG_IO_H
#define _TLOG_IO_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include "tlog_rc.h"
#include "tlog_misc.h"
#include "tlog_stream.h"

/** Minimum value of I/O buffer size */
#define TLOG_IO_SIZE_MIN    TLOG_STREAM_SIZE_MIN

/** I/O buffer */
struct tlog_io {
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

/**
 * Initialize an I/O buffer.
 *
 * @param io    The I/O buffer to initialize.
 * @param size  Size of I/O buffer.
 *
 * @return Status code.
 */
extern tlog_rc tlog_io_init(struct tlog_io *io, size_t size);

/**
 * Check if an I/O buffer is valid.
 *
 * @param io    The I/O buffer to check.
 *
 * @return True if the I/O buffer is valid, false otherwise.
 */
extern bool tlog_io_is_valid(const struct tlog_io *io);

/**
 * Check if an I/O buffer is empty (no data in buffers, except the incomplete
 * characters).
 *
 * @param io    The I/O buffer to check.
 *
 * @return True if the I/O buffer is empty, false otherwise.
 */
extern bool tlog_io_is_empty(const struct tlog_io *io);

/**
 * Write I/O to an I/O buffer.
 *
 * @param io        The I/O buffer to write to.
 * @param timestamp Input arrival timestamp.
 * @param output    True if writing output, false if input.
 * @param pbuf      Location of/for input buffer pointer.
 * @param plen      Location of/for input buffer length.
 *
 * @return Number of input bytes written.
 */
extern size_t tlog_io_write(struct tlog_io *io,
                            const struct timespec *timestamp,
                            bool output,
                            const uint8_t **pbuf,
                            size_t *plen);

/**
 * Flush an I/O buffer - write metadata records to reserved space and reset
 * runs.
 *
 * @param io    The I/O buffer to flush.
 */
extern void tlog_io_flush(struct tlog_io *io);

/**
 * Cut an I/O buffer - write pending incomplete characters to the buffers.
 *
 * @param io    The I/O buffer to cut.
 *
 * @return True if all incomplete characters fit into the remaining space,
 *         false otherwise.
 */
extern bool tlog_io_cut(struct tlog_io *io);

/**
 * Empty an I/O buffer contents (but not pending incomplete characters).
 *
 * @param io        The I/O buffer to empty.
 */
extern void tlog_io_empty(struct tlog_io *io);

/**
 * Cleanup an I/O buffer (free any allocated data).
 *
 * @param io    The I/O buffer to cleanup.
 */
extern void tlog_io_cleanup(struct tlog_io *io);

#endif /* _TLOG_IO_H */
