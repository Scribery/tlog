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

static
TLOG_TRX_BASIC_ACT_SIG(tlog_chunk)
{
    TLOG_TRX_BASIC_ACT_PROLOGUE(tlog_chunk);
    TLOG_TRX_BASIC_ACT_ON_VAR(rem);
    TLOG_TRX_BASIC_ACT_ON_VAR(timing_ptr);
    TLOG_TRX_BASIC_ACT_ON_VAR(got_ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(first_ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(last_ts);
    TLOG_TRX_BASIC_ACT_ON_OBJ(input);
    TLOG_TRX_BASIC_ACT_ON_OBJ(output);
}

static bool
tlog_chunk_reserve(struct tlog_chunk *chunk, size_t len)
{
    assert(tlog_chunk_is_valid(chunk));
    if (len > chunk->rem)
        return false;
    chunk->rem -= len;
    return true;
}

static void
tlog_chunk_write_timing(struct tlog_chunk *chunk,
                        const uint8_t *ptr, size_t len)
{
    assert(tlog_chunk_is_valid(chunk));
    assert(ptr != NULL || len == 0);
    assert((chunk->timing_ptr - chunk->timing_buf + len) <=
                (chunk->size - chunk->rem));

    memcpy(chunk->timing_ptr, ptr, len);
    chunk->timing_ptr += len;
}

/**
 * Record a new timestamp into an I/O, flush streams and add a delay record,
 * if necessary.
 *
 * @param trx       The transaction to act within.
 * @param chunk     The I/O to record timestamp into.
 * @param timestmp  The timestamp to record.
 *
 * @return True if timestamp fit, false otherwise.
 */
static bool
tlog_chunk_advance(tlog_trx_state trx,
                   struct tlog_chunk *chunk,
                   const struct timespec *ts)
{
    struct timespec delay;
    long sec;
    long msec;
    char delay_buf[32];
    int delay_rc;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_chunk_is_valid(chunk));
    assert(ts != NULL);

    TLOG_TRX_FRAME_BEGIN(trx);

    /* If this is the first write */
    if (!chunk->got_ts) {
        chunk->got_ts = true;
        chunk->first_ts = *ts;
        delay_rc = 0;
    } else {
        assert(tlog_timespec_cmp(ts, &chunk->last_ts) >= 0);
        tlog_timespec_sub(ts, &chunk->last_ts, &delay);
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
        goto failure;
    }
    chunk->last_ts = *ts;

    /* If we need to add a delay record */
    if (delay_rc > 0) {
        tlog_stream_flush(&chunk->input);
        tlog_stream_flush(&chunk->output);
        /* If it doesn't fit */
        if (!tlog_chunk_reserve(chunk, (size_t)delay_rc))
            goto failure;
        tlog_chunk_write_timing(chunk,
                                (uint8_t *)delay_buf, (size_t)delay_rc);
    }

    TLOG_TRX_FRAME_COMMIT(trx);
    return true;

failure:
    TLOG_TRX_FRAME_ABORT(trx);
    return false;
}

/**
 * Reserve space from the dispatcher.
 *
 * @param dispatcher    The dispatcher to reserve the space from.
 * @param len           The amount of space to reserve.
 *
 * @return True if there was enough space, false otherwise.
 */
static bool
tlog_chunk_dispatcher_reserve(struct tlog_dispatcher *dispatcher,
                              size_t len)
{
    struct tlog_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                 struct tlog_chunk,
                                                 dispatcher);
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(tlog_chunk_is_valid(chunk));
    return tlog_chunk_reserve(chunk, len);
}

/**
 * Write into the metadata space reserved from the dispatcher.
 *
 * @param dispatcher    The dispatcher to print to.
 * @param ptr           Pointer to the data to write.
 * @param len           Length of the data to write.
 */
static void
tlog_chunk_dispatcher_write(struct tlog_dispatcher *dispatcher,
                            const uint8_t *ptr, size_t len)
{
    struct tlog_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                 struct tlog_chunk,
                                                 dispatcher);
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(tlog_chunk_is_valid(chunk));
    assert(ptr != NULL || len == 0);
    tlog_chunk_write_timing(chunk, ptr, len);
}

/**
 * Advance dispatcher time.
 *
 * @param trx           The transaction to act within.
 * @param dispatcher    The dispatcher to advance time for.
 * @param ts            The time to advance to, must be equal or greater than
 *                      the previously advanced to.
 *
 * @return True if there was space to record the advanced time, false
 *         otherwise.
 */
static bool
tlog_chunk_dispatcher_advance(tlog_trx_state trx,
                              struct tlog_dispatcher *dispatcher,
                              const struct timespec *ts)
{
    struct tlog_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                 struct tlog_chunk,
                                                 dispatcher);
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(tlog_chunk_is_valid(chunk));
    assert(ts != NULL);
    return tlog_chunk_advance(trx, chunk, ts);
}

tlog_grc
tlog_chunk_init(struct tlog_chunk *chunk, size_t size)
{
    tlog_grc grc;

    assert(chunk != NULL);
    assert(size >= TLOG_CHUNK_SIZE_MIN);

    memset(chunk, 0, sizeof(*chunk));

    chunk->trx_iface = TLOG_TRX_BASIC_IFACE(tlog_chunk);
    tlog_dispatcher_init(&chunk->dispatcher,
                         tlog_chunk_dispatcher_advance,
                         tlog_chunk_dispatcher_reserve,
                         tlog_chunk_dispatcher_write,
                         chunk, &chunk->trx_iface);
    chunk->size = size;
    chunk->rem = size;

    grc = tlog_stream_init(&chunk->input, &chunk->dispatcher,
                           size, '<', '[');
    if (grc != TLOG_RC_OK)
        goto error;
    grc = tlog_stream_init(&chunk->output, &chunk->dispatcher,
                           size, '>', ']');
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
           tlog_dispatcher_is_valid(&chunk->dispatcher) &&
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
 * Write a window packet payload to a chunk.
 *
 * @param trx       The transaction to act within.
 * @param chunk     The chunk to write to.
 * @param pkt       The packet to write the payload of.
 * @param ppos      Location of position in the packet the write should start
 *                  at (set to 0 on first write) / location for (opaque)
 *                  position in the packet the write ended at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_chunk_write_window(tlog_trx_state trx,
                        struct tlog_chunk *chunk,
                        const struct tlog_pkt *pkt,
                        size_t *ppos)
{
    /* Window string buffer (max: "=65535x65535") */
    char buf[16];
    int rc;
    size_t len;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(pkt->type == TLOG_PKT_TYPE_WINDOW);
    assert(ppos != NULL);
    assert(*ppos <= 1);

    if (*ppos >= 1)
        return true;

    TLOG_TRX_FRAME_BEGIN(trx);

    rc = snprintf(buf, sizeof(buf), "=%hux%hu",
                  pkt->data.window.width, pkt->data.window.height);
    if (rc < 0) {
        assert(false);
        goto failure;
    }

    len = (size_t)rc;
    if (len >= sizeof(buf)) {
        assert(false);
        goto failure;
    }

    if (len > chunk->rem)
        goto failure;

    tlog_stream_flush(&chunk->input);
    tlog_stream_flush(&chunk->output);

    if (!tlog_chunk_advance(trx, chunk, &pkt->timestamp))
        goto failure;

    if (!tlog_chunk_reserve(chunk, len))
        goto failure;

    tlog_chunk_write_timing(chunk, (uint8_t *)buf, len);

    *ppos = 1;
    TLOG_TRX_FRAME_COMMIT(trx);
    return true;

failure:
    TLOG_TRX_FRAME_ABORT(trx);
    return false;
}

/**
 * Write an I/O packet payload to a chunk.
 *
 * @param trx       The transaction to act within.
 * @param chunk     The chunk to write to.
 * @param pkt       The packet to write the payload of.
 * @param ppos      Location of position in the packet the write should start
 *                  at (set to 0 on first write) / location for (opaque)
 *                  position in the packet the write ended at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_chunk_write_io(tlog_trx_state trx,
                    struct tlog_chunk *chunk,
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
    *ppos += tlog_stream_write(trx,
                               pkt->data.io.output
                                    ? &chunk->output
                                    : &chunk->input,
                               &pkt->timestamp,
                               &buf, &len);
    return len == 0;
}

bool
tlog_chunk_write(struct tlog_chunk *chunk,
                 const struct tlog_pkt *pkt,
                 size_t *ppos)
{
    tlog_trx_state trx = TLOG_TRX_STATE_ROOT;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    size_t pos;
    bool complete;

    assert(tlog_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    assert(ppos != NULL);

    TLOG_TRX_FRAME_BEGIN(trx);

    /* Write (a part of) the packet */
    pos = *ppos;
    switch (pkt->type) {
        case TLOG_PKT_TYPE_IO:
            complete = tlog_chunk_write_io(trx, chunk, pkt, &pos);
            break;
        case TLOG_PKT_TYPE_WINDOW:
            complete = tlog_chunk_write_window(trx, chunk, pkt, &pos);
            break;
        default:
            assert(false);
            complete = false;
            break;
    }
    assert(pos >= *ppos);

    /* If no part of the packet fits */
    if (!complete && pos == *ppos) {
        TLOG_TRX_FRAME_ABORT(trx);
        return false;
    }

    TLOG_TRX_FRAME_COMMIT(trx);
    *ppos = pos;
    return complete;
}

void
tlog_chunk_flush(struct tlog_chunk *chunk)
{
    assert(tlog_chunk_is_valid(chunk));
    tlog_stream_flush(&chunk->input);
    tlog_stream_flush(&chunk->output);
}

bool
tlog_chunk_cut(struct tlog_chunk *chunk)
{
    tlog_trx_state trx = TLOG_TRX_STATE_ROOT;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_chunk_is_valid(chunk));

    TLOG_TRX_FRAME_BEGIN(trx);

    /* Cut the streams */
    if (!tlog_stream_cut(trx, &chunk->input) ||
        !tlog_stream_cut(trx, &chunk->output)) {
        TLOG_TRX_FRAME_ABORT(trx);
        return false;
    }

    TLOG_TRX_FRAME_COMMIT(trx);
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
    chunk->got_ts = false;
    tlog_timespec_zero(&chunk->first_ts);
    tlog_timespec_zero(&chunk->last_ts);
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
