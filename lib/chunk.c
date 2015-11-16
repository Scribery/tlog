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
#include <tlog/chunk.h>

tlog_grc
tlog_chunk_init(struct tlog_chunk *chunk, size_t size)
{
    tlog_grc grc;

    assert(chunk != NULL);
    assert(size >= TLOG_CHUNK_SIZE_MIN);

    memset(chunk, 0, sizeof(*chunk));

    chunk->size = size;
    chunk->rem = size;

    grc = tlog_stream_init(&chunk->input, size, '<', '[');
    if (grc != TLOG_RC_OK)
        goto error;
    grc = tlog_stream_init(&chunk->output, size, '>', ']');
    if (grc != TLOG_RC_OK)
        goto error;
    chunk->timing_buf = malloc(size);
    if (chunk->timing_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }
    chunk->timing_ptr = chunk->timing_buf;

    assert(tlog_chunk_is_valid(chunk));
    return TLOG_RC_OK;

error:
    tlog_chunk_cleanup(chunk);
    return grc;
}

bool
tlog_chunk_is_valid(const struct tlog_chunk *chunk)
{
    return chunk != NULL &&
           chunk->size >= TLOG_CHUNK_SIZE_MIN &&
           tlog_stream_is_valid(&chunk->input) &&
           tlog_stream_is_valid(&chunk->output) &&
           chunk->timing_buf != NULL &&
           chunk->timing_ptr >= chunk->timing_buf &&
           chunk->timing_ptr <= chunk->timing_buf + chunk->size &&
           chunk->rem <= chunk->size &&
           ((chunk->timing_ptr - chunk->timing_buf) +
            chunk->input.txt_len + chunk->input.bin_len +
            chunk->output.txt_len + chunk->output.bin_len) <=
                (chunk->size - chunk->rem);
}

bool
tlog_chunk_is_pending(const struct tlog_chunk *chunk)
{
    assert(tlog_chunk_is_valid(chunk));
    return tlog_stream_is_pending(&chunk->input) ||
           tlog_stream_is_pending(&chunk->output);
}

bool
tlog_chunk_is_empty(const struct tlog_chunk *chunk)
{
    assert(tlog_chunk_is_valid(chunk));
    return tlog_stream_is_empty(&chunk->input) &&
           tlog_stream_is_empty(&chunk->output);
}

/**
 * Record a new timestamp into an I/O, flush streams and add a delay record,
 * if necessary.
 *
 * @param chunk        The I/O to record timestamp into.
 * @param timestmp  The timestamp to record.
 *
 * @return True if timestamp fit, false otherwise.
 */
static bool
tlog_chunk_timestamp(struct tlog_chunk *chunk,
                     const struct timespec *timestamp)
{
    struct timespec delay;
    long sec;
    long msec;
    char delay_buf[32];
    int delay_rc;

    assert(tlog_chunk_is_valid(chunk));
    assert(timestamp != NULL);

    /* If this is the first write */
    if (tlog_chunk_is_empty(chunk) && !tlog_chunk_is_pending(chunk)) {
        chunk->first = *timestamp;
        delay_rc = 0;
    } else {
        tlog_timespec_sub(timestamp, &chunk->last, &delay);
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
    chunk->last = *timestamp;

    /* If we need to add a delay record */
    if (delay_rc > 0) {
        /* If it doesn't fit */
        if ((size_t)delay_rc > chunk->rem)
            return false;
        tlog_stream_flush(&chunk->input, &chunk->timing_ptr);
        tlog_stream_flush(&chunk->output, &chunk->timing_ptr);
        memcpy(chunk->timing_ptr, delay_buf, (size_t)delay_rc);
        chunk->timing_ptr += delay_rc;
        chunk->rem -= delay_rc;
    }

    return true;
}

size_t
tlog_chunk_write(struct tlog_chunk *chunk, const struct timespec *timestamp,
                 bool output, const uint8_t **pbuf, size_t *plen)
{
    tlog_trx trx = TLOG_TRX_INIT;
    TLOG_TRX_STORE_DECL(tlog_chunk);
    size_t written;

    assert(tlog_chunk_is_valid(chunk));
    assert(timestamp != NULL);
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);

    if (*plen == 0)
        return 0;

    TLOG_TRX_BEGIN(&trx, tlog_chunk, chunk);

    /* Record new timestamp */
    if (!tlog_chunk_timestamp(chunk, timestamp)) {
        TLOG_TRX_ABORT(&trx, tlog_chunk, chunk);
        return 0;
    }

    /* Write as much I/O data as we can */
    written = tlog_stream_write(output ? &chunk->output : &chunk->input,
                                pbuf, plen, &chunk->timing_ptr, &chunk->rem);
    /* If no I/O data fits */
    if (written == 0) {
        TLOG_TRX_ABORT(&trx, tlog_chunk, chunk);
        return 0;
    }

    TLOG_TRX_COMMIT(&trx);
    return written;
}

void
tlog_chunk_flush(struct tlog_chunk *chunk)
{
    assert(tlog_chunk_is_valid(chunk));
    tlog_stream_flush(&chunk->input, &chunk->timing_ptr);
    tlog_stream_flush(&chunk->output, &chunk->timing_ptr);
}

bool
tlog_chunk_cut(struct tlog_chunk *chunk)
{
    tlog_trx trx = TLOG_TRX_INIT;
    TLOG_TRX_STORE_DECL(tlog_chunk);

    assert(tlog_chunk_is_valid(chunk));

    TLOG_TRX_BEGIN(&trx, tlog_chunk, chunk);

    /* Cut the streams */
    if (!tlog_stream_cut(&chunk->input, &chunk->timing_ptr, &chunk->rem) ||
        !tlog_stream_cut(&chunk->output, &chunk->timing_ptr, &chunk->rem)) {
        TLOG_TRX_ABORT(&trx, tlog_chunk, chunk);
        return false;
    }

    TLOG_TRX_COMMIT(&trx);
    return true;
}

void
tlog_chunk_empty(struct tlog_chunk *chunk)
{
    assert(tlog_chunk_is_valid(chunk));
    chunk->rem = chunk->size;
    chunk->timing_ptr = chunk->timing_buf;
    tlog_stream_empty(&chunk->input);
    tlog_stream_empty(&chunk->output);
    tlog_timespec_zero(&chunk->first);
    tlog_timespec_zero(&chunk->last);
    assert(tlog_chunk_is_valid(chunk));
}

void
tlog_chunk_cleanup(struct tlog_chunk *chunk)
{
    assert(chunk != NULL);
    tlog_stream_cleanup(&chunk->input);
    tlog_stream_cleanup(&chunk->output);
    free(chunk->timing_buf);
    chunk->timing_buf = NULL;
}
