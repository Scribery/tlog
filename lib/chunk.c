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
    return chunk->rem >= chunk->size;
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
    if (delay_rc < 0 || (size_t)delay_rc >= sizeof(delay_buf)) {
        assert(false);
        return false;
    }
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

/**
 * Write a window packet payload to a chunk.
 *
 * @param chunk     The chunk to write to.
 * @param pkt       The packet to write the payload of.
 * @param ppos      Location of position in the packet the write should start
 *                  at (set to 0 on first write) / location for (opaque)
 *                  position in the packet the write ended at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_chunk_write_window(struct tlog_chunk *chunk,
                        const struct tlog_pkt *pkt,
                        size_t *ppos)
{
    /* Window string buffer (max: "=65535x65535") */
    char buf[16];
    int rc;
    size_t len;

    assert(tlog_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(pkt->type == TLOG_PKT_TYPE_WINDOW);
    assert(ppos != NULL);
    assert(*ppos <= 1);

    if (*ppos >= 1)
        return true;

    rc = snprintf(buf, sizeof(buf), "=%hux%hu",
                  pkt->data.window.width, pkt->data.window.height);
    if (rc < 0) {
        assert(false);
        return false;
    }

    len = (size_t)rc;
    if (len >= sizeof(buf)) {
        assert(false);
        return false;
    }

    if (len > chunk->rem)
        return false;

    tlog_stream_flush(&chunk->input, &chunk->timing_ptr);
    tlog_stream_flush(&chunk->output, &chunk->timing_ptr);

    memcpy(chunk->timing_ptr, buf, len);
    chunk->timing_ptr += len;
    chunk->rem -= len;

    *ppos = 1;
    return true;
}

/**
 * Write an I/O packet payload to a chunk.
 *
 * @param chunk     The chunk to write to.
 * @param pkt       The packet to write the payload of.
 * @param ppos      Location of position in the packet the write should start
 *                  at (set to 0 on first write) / location for (opaque)
 *                  position in the packet the write ended at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_chunk_write_io(struct tlog_chunk *chunk,
                    const struct tlog_pkt *pkt,
                    size_t *ppos)
{
    const uint8_t *buf;
    size_t len;

    assert(tlog_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(pkt->type == TLOG_PKT_TYPE_IO);
    assert(ppos != NULL);
    assert(*ppos <= pkt->data.io.len);

    if (*ppos >= pkt->data.io.len)
        return true;

    buf = pkt->data.io.buf + *ppos;
    len = pkt->data.io.len - *ppos;
    *ppos += tlog_stream_write(pkt->data.io.output
                                    ? &chunk->output
                                    : &chunk->input,
                               &buf, &len,
                               &chunk->timing_ptr, &chunk->rem);
    return len == 0;
}

bool
tlog_chunk_write(struct tlog_chunk *chunk,
                 const struct tlog_pkt *pkt,
                 size_t *ppos)
{
    tlog_trx trx = TLOG_TRX_INIT;
    TLOG_TRX_STORE_DECL(tlog_chunk);
    size_t pos;
    bool complete;

    assert(tlog_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    assert(ppos != NULL);

    TLOG_TRX_BEGIN(&trx, tlog_chunk, chunk);

    /* Record the timestamp (if it's new) */
    if (!tlog_chunk_timestamp(chunk, &pkt->timestamp)) {
        TLOG_TRX_ABORT(&trx, tlog_chunk, chunk);
        return false;
    }

    /* Write (a part of) the packet */
    pos = *ppos;
    switch (pkt->type) {
        case TLOG_PKT_TYPE_IO:
            complete = tlog_chunk_write_io(chunk, pkt, &pos);
            break;
        case TLOG_PKT_TYPE_WINDOW:
            complete = tlog_chunk_write_window(chunk, pkt, &pos);
            break;
        default:
            assert(false);
            complete = false;
            break;
    }
    assert(pos >= *ppos);

    /* If no part of the packet fits */
    if (!complete && pos == *ppos) {
        /* Revert the possible timestamp writing */
        TLOG_TRX_ABORT(&trx, tlog_chunk, chunk);
        return false;
    }

    TLOG_TRX_COMMIT(&trx);
    *ppos = pos;
    return complete;
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
