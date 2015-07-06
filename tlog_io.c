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

#include <errno.h>
#include <stdio.h>
#include "tlog_io.h"

tlog_rc
tlog_io_init(struct tlog_io *io, size_t size)
{
    int orig_errno;

    assert(io != NULL);
    assert(size >= TLOG_IO_SIZE_MIN);

    memset(io, 0, sizeof(*io));

    io->size = size;
    io->rem = size;

    if (tlog_stream_init(&io->input, size, '<', '[') != TLOG_RC_OK)
        goto error;
    if (tlog_stream_init(&io->output, size, '>', ']') != TLOG_RC_OK)
        goto error;
    io->timing_buf = malloc(size);
    if (io->timing_buf == NULL)
        goto error;
    io->timing_ptr = io->timing_buf;

    return TLOG_RC_OK;

error:
    orig_errno = errno;
    tlog_io_cleanup(io);
    errno = orig_errno;
    return TLOG_RC_FAILURE;
}

bool
tlog_io_is_valid(const struct tlog_io *io)
{
    return io != NULL &&
           io->size >= TLOG_IO_SIZE_MIN &&
           tlog_stream_is_valid(&io->input) &&
           tlog_stream_is_valid(&io->output) &&
           io->timing_buf != NULL &&
           io->rem <= io->size &&
           ((io->timing_ptr - io->timing_buf) +
            io->input.txt_len + io->input.bin_len +
            io->output.txt_len + io->output.bin_len) <= (io->size - io->rem);
}

bool
tlog_io_is_pending(const struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    return tlog_stream_is_pending(&io->input) ||
           tlog_stream_is_pending(&io->output);
}

bool
tlog_io_is_empty(const struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    return tlog_stream_is_empty(&io->input) &&
           tlog_stream_is_empty(&io->output);
}

/**
 * Record a new timestamp into an I/O, flush streams and add a delay record,
 * if necessary.
 *
 * @param io        The I/O to record timestamp into.
 * @param timestmp  The timestamp to record.
 *
 * @return True if timestamp fit, false otherwise.
 */
static bool
tlog_io_timestamp(struct tlog_io *io, const struct timespec *timestamp)
{
    struct timespec delay;
    long sec;
    long msec;
    char delay_buf[32];
    int delay_rc;

    assert(tlog_io_is_valid(io));
    assert(timestamp != NULL);

    /* If this is the first write */
    if (tlog_io_is_empty(io) && !tlog_io_is_pending(io)) {
        io->first = *timestamp;
        delay_rc = 0;
    } else {
        tlog_timespec_sub(timestamp, &io->last, &delay);
        sec = (long)delay.tv_sec;
        msec = delay.tv_nsec / 1000000;
        if (sec != 0) {
            delay_rc = snprintf(delay_buf, sizeof(delay_buf),
                                "+%ld%03ld", sec, msec);
        } else if (msec != 0) {
            delay_rc = snprintf(delay_buf, sizeof(delay_buf),
                                "+%ld", msec);
        } else {
            delay_rc = 0;
        }
    }
    assert(delay_rc >= 0);
    assert((size_t)delay_rc < sizeof(delay_buf));
    io->last = *timestamp;

    /* If we need to add a delay record */
    if (delay_rc > 0) {
        /* If it doesn't fit */
        if ((size_t)delay_rc > io->rem)
            return false;
        tlog_stream_flush(&io->input, &io->timing_ptr);
        tlog_stream_flush(&io->output, &io->timing_ptr);
        memcpy(io->timing_ptr, delay_buf, (size_t)delay_rc);
        io->timing_ptr += delay_rc;
        io->rem -= delay_rc;
    }

    return true;
}

size_t
tlog_io_write(struct tlog_io *io, const struct timespec *timestamp,
              bool output, const uint8_t **pbuf, size_t *plen)
{
    tlog_trx trx = TLOG_TRX_INIT;
    TLOG_TRX_STORE_DECL(tlog_io);
    size_t written;

    assert(tlog_io_is_valid(io));
    assert(timestamp != NULL);
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);

    if (*plen == 0)
        return 0;

    TLOG_TRX_BEGIN(&trx, tlog_io, io);

    /* Record new timestamp */
    if (!tlog_io_timestamp(io, timestamp)) {
        TLOG_TRX_ABORT(&trx, tlog_io, io);
        return 0;
    }

    /* Write as much I/O data as we can */
    written = tlog_stream_write(output ? &io->output : &io->input,
                                pbuf, plen, &io->timing_ptr, &io->rem);
    /* If no I/O data fits */
    if (written == 0) {
        TLOG_TRX_ABORT(&trx, tlog_io, io);
        return 0;
    }

    TLOG_TRX_COMMIT(&trx);
    return written;
}

void
tlog_io_flush(struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    tlog_stream_flush(&io->input, &io->timing_ptr);
    tlog_stream_flush(&io->output, &io->timing_ptr);
}

bool
tlog_io_cut(struct tlog_io *io)
{
    tlog_trx trx = TLOG_TRX_INIT;
    TLOG_TRX_STORE_DECL(tlog_io);

    assert(tlog_io_is_valid(io));

    TLOG_TRX_BEGIN(&trx, tlog_io, io);

    /* Cut the streams */
    if (!tlog_stream_cut(&io->input, &io->timing_ptr, &io->rem) ||
        !tlog_stream_cut(&io->output, &io->timing_ptr, &io->rem)) {
        TLOG_TRX_ABORT(&trx, tlog_io, io);
        return false;
    }

    TLOG_TRX_COMMIT(&trx);
    return true;
}

void
tlog_io_empty(struct tlog_io *io)
{
    assert(tlog_io_is_valid(io));
    io->rem = io->size;
    io->timing_ptr = io->timing_buf;
    tlog_stream_empty(&io->input);
    tlog_stream_empty(&io->output);
    tlog_timespec_zero(&io->first);
    tlog_timespec_zero(&io->last);
    assert(tlog_io_is_valid(io));
}

void
tlog_io_cleanup(struct tlog_io *io)
{
    assert(io != NULL);
    tlog_stream_cleanup(&io->input);
    tlog_stream_cleanup(&io->output);
    free(io->timing_buf);
    io->timing_buf = NULL;
}
