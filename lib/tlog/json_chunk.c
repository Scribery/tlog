/*
 * JSON encoder data chunk buffer.
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
#include <tlog/timespec.h>
#include <tlog/json_chunk.h>

static
TLOG_TRX_BASIC_ACT_SIG(tlog_json_chunk)
{
    TLOG_TRX_BASIC_ACT_PROLOGUE(tlog_json_chunk);
    TLOG_TRX_BASIC_ACT_ON_VAR(rem);
    TLOG_TRX_BASIC_ACT_ON_VAR(timing_ptr);
    TLOG_TRX_BASIC_ACT_ON_VAR(got_ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(first_ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(last_ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(window_state);
    TLOG_TRX_BASIC_ACT_ON_VAR(last_width);
    TLOG_TRX_BASIC_ACT_ON_VAR(last_height);
    TLOG_TRX_BASIC_ACT_ON_OBJ(input);
    TLOG_TRX_BASIC_ACT_ON_OBJ(output);
}

/**
 * Print window coordinates to a string, in timing format.
 *
 * @param buf       The buffer to print to, can be NULL if size is 0.
 * @param size      The size of the buffer to print to, includes terminating zero.
 * @param width     The window width to print.
 * @param height    The window height to print.
 *
 * @return Number of characters (to be) printed, excluding terminating zero.
 */
static size_t
tlog_json_chunk_sprint_window(uint8_t *buf, size_t size,
                              unsigned short int width,
                              unsigned short int height)
{
    int rc;
    assert(buf != NULL || size == 0);
    rc = snprintf((char *)buf, size, "=%hux%hu", width, height);
    assert(rc >= 0);
    return (size_t)rc;
}

static bool
tlog_json_chunk_reserve(struct tlog_json_chunk *chunk, size_t len)
{
    assert(tlog_json_chunk_is_valid(chunk));

    /* If we'll have to write the initial window */
    if (chunk->window_state == TLOG_JSON_CHUNK_WINDOW_STATE_KNOWN) {
        len += tlog_json_chunk_sprint_window(NULL, 0,
                                             chunk->last_width,
                                             chunk->last_height);
    }
    if (len > chunk->rem) {
        return false;
    }
    chunk->rem -= len;
    if (chunk->window_state == TLOG_JSON_CHUNK_WINDOW_STATE_KNOWN) {
        chunk->window_state = TLOG_JSON_CHUNK_WINDOW_STATE_RESERVED;
    }
    return true;
}

static void
tlog_json_chunk_write_timing_raw(struct tlog_json_chunk *chunk,
                                 const uint8_t *ptr, size_t len)
{
    assert(tlog_json_chunk_is_valid(chunk));
    assert(ptr != NULL || len == 0);
    assert((chunk->timing_ptr - chunk->timing_buf + len) <=
                (chunk->size - chunk->rem));

    memcpy(chunk->timing_ptr, ptr, len);
    chunk->timing_ptr += len;
}

static void
tlog_json_chunk_write_timing(struct tlog_json_chunk *chunk,
                             const uint8_t *ptr, size_t len)
{
    /* If we have to write the (reserved) initial window */
    if (chunk->window_state == TLOG_JSON_CHUNK_WINDOW_STATE_RESERVED) {
        /* Window string buffer (max: "=65535x65535") */
        uint8_t buf[16];
        size_t len;
        len = tlog_json_chunk_sprint_window(buf, sizeof(buf),
                                            chunk->last_width,
                                            chunk->last_height);
        assert(len < sizeof(buf));
        tlog_json_chunk_write_timing_raw(chunk, buf, len);
        chunk->window_state = TLOG_JSON_CHUNK_WINDOW_STATE_WRITTEN;
    }
    tlog_json_chunk_write_timing_raw(chunk, ptr, len);
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
tlog_json_chunk_advance(tlog_trx_state trx,
                        struct tlog_json_chunk *chunk,
                        const struct timespec *ts)
{
    char delay_buf[32];
    int delay_rc;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_json_chunk_is_valid(chunk));
    assert(ts != NULL);

    TLOG_TRX_FRAME_BEGIN(trx);

    /* If this is the first write */
    if (!chunk->got_ts) {
        chunk->got_ts = true;
        chunk->first_ts = *ts;
        chunk->last_ts = *ts;
        delay_rc = 0;
    } else {
        if (tlog_timespec_cmp(ts, &chunk->last_ts) > 0) {
            struct timespec delay;
            long sec;
            long msec;

            tlog_timespec_sub(ts, &chunk->last_ts, &delay);
            chunk->last_ts = *ts;
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
        } else {
            delay_rc = 0;
        }
    }
    if (delay_rc < 0 || (size_t)delay_rc >= sizeof(delay_buf)) {
        assert(false);
        goto failure;
    }

    /* If we need to add a delay record */
    if (delay_rc > 0) {
        tlog_json_stream_flush(&chunk->input);
        tlog_json_stream_flush(&chunk->output);
        /* If it doesn't fit */
        if (!tlog_json_chunk_reserve(chunk, (size_t)delay_rc)) {
            goto failure;
        }
        tlog_json_chunk_write_timing(chunk,
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
tlog_json_chunk_dispatcher_reserve(struct tlog_json_dispatcher *dispatcher,
                                   size_t len)
{
    struct tlog_json_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                      struct tlog_json_chunk,
                                                      dispatcher);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(tlog_json_chunk_is_valid(chunk));
    return tlog_json_chunk_reserve(chunk, len);
}

/**
 * Write into the metadata space reserved from the dispatcher.
 *
 * @param dispatcher    The dispatcher to print to.
 * @param ptr           Pointer to the data to write.
 * @param len           Length of the data to write.
 */
static void
tlog_json_chunk_dispatcher_write(struct tlog_json_dispatcher *dispatcher,
                                 const uint8_t *ptr, size_t len)
{
    struct tlog_json_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                      struct tlog_json_chunk,
                                                      dispatcher);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(tlog_json_chunk_is_valid(chunk));
    assert(ptr != NULL || len == 0);
    tlog_json_chunk_write_timing(chunk, ptr, len);
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
tlog_json_chunk_dispatcher_advance(tlog_trx_state trx,
                                   struct tlog_json_dispatcher *dispatcher,
                                   const struct timespec *ts)
{
    struct tlog_json_chunk *chunk = TLOG_CONTAINER_OF(dispatcher,
                                                      struct tlog_json_chunk,
                                                      dispatcher);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(tlog_json_chunk_is_valid(chunk));
    assert(ts != NULL);
    return tlog_json_chunk_advance(trx, chunk, ts);
}

tlog_grc
tlog_json_chunk_init(struct tlog_json_chunk *chunk, size_t size)
{
    tlog_grc grc;

    assert(chunk != NULL);
    assert(size >= TLOG_JSON_CHUNK_SIZE_MIN);

    memset(chunk, 0, sizeof(*chunk));

    chunk->trx_iface = TLOG_TRX_BASIC_IFACE(tlog_json_chunk);
    tlog_json_dispatcher_init(&chunk->dispatcher,
                              tlog_json_chunk_dispatcher_advance,
                              tlog_json_chunk_dispatcher_reserve,
                              tlog_json_chunk_dispatcher_write,
                              chunk, &chunk->trx_iface);
    chunk->size = size;
    chunk->rem = size;

    grc = tlog_json_stream_init(&chunk->input, &chunk->dispatcher,
                                size, '<', '[');
    if (grc != TLOG_RC_OK) {
        goto error;
    }
    grc = tlog_json_stream_init(&chunk->output, &chunk->dispatcher,
                                size, '>', ']');
    if (grc != TLOG_RC_OK) {
        goto error;
    }
    chunk->timing_buf = malloc(size);
    if (chunk->timing_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }
    chunk->timing_ptr = chunk->timing_buf;

    assert(tlog_json_chunk_is_valid(chunk));
    return TLOG_RC_OK;

error:
    tlog_json_chunk_cleanup(chunk);
    return grc;
}

bool
tlog_json_chunk_is_valid(const struct tlog_json_chunk *chunk)
{
    return chunk != NULL &&
           tlog_json_dispatcher_is_valid(&chunk->dispatcher) &&
           chunk->size >= TLOG_JSON_CHUNK_SIZE_MIN &&
           tlog_json_stream_is_valid(&chunk->input) &&
           tlog_json_stream_is_valid(&chunk->output) &&
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
tlog_json_chunk_is_pending(const struct tlog_json_chunk *chunk)
{
    assert(tlog_json_chunk_is_valid(chunk));
    return tlog_json_stream_is_pending(&chunk->input) ||
           tlog_json_stream_is_pending(&chunk->output);
}

bool
tlog_json_chunk_is_empty(const struct tlog_json_chunk *chunk)
{
    assert(tlog_json_chunk_is_valid(chunk));
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
 * @param end       Position in the packet the write should end at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_json_chunk_write_window(tlog_trx_state trx,
                             struct tlog_json_chunk *chunk,
                             const struct tlog_pkt *pkt,
                             struct tlog_pkt_pos *ppos,
                             const struct tlog_pkt_pos *end)
{
    /* Window string buffer (max: "=65535x65535") */
    uint8_t buf[16];
    size_t len;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_json_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(pkt->type == TLOG_PKT_TYPE_WINDOW);
    assert(tlog_pkt_pos_is_valid(ppos));
    assert(tlog_pkt_pos_is_compatible(ppos, pkt));
    assert(tlog_pkt_pos_is_reachable(ppos, pkt));
    assert(tlog_pkt_pos_is_valid(end));
    assert(tlog_pkt_pos_is_compatible(end, pkt));
    assert(tlog_pkt_pos_is_reachable(end, pkt));

    if (tlog_pkt_pos_cmp(ppos, end) >= 0) {
        return true;
    }

    TLOG_TRX_FRAME_BEGIN(trx);

    if (chunk->window_state >= TLOG_JSON_CHUNK_WINDOW_STATE_KNOWN) {
        if (pkt->data.window.width == chunk->last_width &&
            pkt->data.window.height == chunk->last_height) {
            goto success;
        }
    }

    len = tlog_json_chunk_sprint_window(buf, sizeof(buf),
                                        pkt->data.window.width,
                                        pkt->data.window.height);
    assert(len < sizeof(buf));
    if (len >= sizeof(buf)) {
        goto failure;
    }

    tlog_json_stream_flush(&chunk->input);
    tlog_json_stream_flush(&chunk->output);

    if (!tlog_json_chunk_advance(trx, chunk, &pkt->timestamp)) {
        goto failure;
    }

    /* Signal we're reserving space for a window right now */
    chunk->window_state = TLOG_JSON_CHUNK_WINDOW_STATE_RESERVED;
    if (!tlog_json_chunk_reserve(chunk, len)) {
        goto failure;
    }

    /* Signal we're writing a window right now */
    chunk->window_state = TLOG_JSON_CHUNK_WINDOW_STATE_WRITTEN;
    tlog_json_chunk_write_timing(chunk, buf, len);

    chunk->last_width = pkt->data.window.width;
    chunk->last_height = pkt->data.window.height;

success:
    tlog_pkt_pos_move_past(ppos, pkt);
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
 * @param end       Position in the packet the write should end at.
 *
 * @return True if the whole of the (remaining) packet fit into the chunk.
 */
static bool
tlog_json_chunk_write_io(tlog_trx_state trx,
                         struct tlog_json_chunk *chunk,
                         const struct tlog_pkt *pkt,
                         struct tlog_pkt_pos *ppos,
                         const struct tlog_pkt_pos *end)
{
    const uint8_t *buf;
    size_t len;

    assert(tlog_json_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(pkt->type == TLOG_PKT_TYPE_IO);
    assert(tlog_pkt_pos_is_valid(ppos));
    assert(tlog_pkt_pos_is_compatible(ppos, pkt));
    assert(tlog_pkt_pos_is_reachable(ppos, pkt));
    assert(tlog_pkt_pos_is_valid(end));
    assert(tlog_pkt_pos_is_compatible(end, pkt));
    assert(tlog_pkt_pos_is_reachable(end, pkt));

    if (tlog_pkt_pos_cmp(ppos, end) >= 0) {
        return true;
    }

    buf = pkt->data.io.buf + ppos->val;
    len = end->val - ppos->val;
    tlog_pkt_pos_move(ppos, pkt,
                      tlog_json_stream_write(trx,
                                             pkt->data.io.output
                                                  ? &chunk->output
                                                  : &chunk->input,
                                             &pkt->timestamp,
                                             &buf, &len));
    return len == 0;
}

bool
tlog_json_chunk_write(struct tlog_json_chunk *chunk,
                      const struct tlog_pkt *pkt,
                      struct tlog_pkt_pos *ppos,
                      const struct tlog_pkt_pos *end)
{
    tlog_trx_state trx = TLOG_TRX_STATE_ROOT;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    struct tlog_pkt_pos pos;
    bool complete;

    assert(tlog_json_chunk_is_valid(chunk));
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    assert(tlog_pkt_pos_is_valid(ppos));
    assert(tlog_pkt_pos_is_compatible(ppos, pkt));
    assert(tlog_pkt_pos_is_reachable(ppos, pkt));
    assert(tlog_pkt_pos_is_valid(end));
    assert(tlog_pkt_pos_is_compatible(end, pkt));
    assert(tlog_pkt_pos_is_reachable(end, pkt));
    assert(!chunk->got_ts ||
           tlog_timespec_cmp(&chunk->last_ts, &pkt->timestamp) <= 0);

    TLOG_TRX_FRAME_BEGIN(trx);

    /* Write (a part of) the packet */
    pos = *ppos;
    switch (pkt->type) {
        case TLOG_PKT_TYPE_IO:
            complete = tlog_json_chunk_write_io(trx, chunk, pkt,
                                                &pos, end);
            break;
        case TLOG_PKT_TYPE_WINDOW:
            complete = tlog_json_chunk_write_window(trx, chunk, pkt,
                                                    &pos, end);
            break;
        default:
            assert(false);
            complete = false;
            break;
    }
    assert(tlog_pkt_pos_cmp(&pos, ppos) >= 0);

    /* If no part of the packet fits */
    if (!complete && tlog_pkt_pos_cmp(&pos, ppos) == 0) {
        TLOG_TRX_FRAME_ABORT(trx);
        return false;
    }

    TLOG_TRX_FRAME_COMMIT(trx);
    *ppos = pos;
    return complete;
}

void
tlog_json_chunk_flush(struct tlog_json_chunk *chunk)
{
    assert(tlog_json_chunk_is_valid(chunk));
    tlog_json_stream_flush(&chunk->input);
    tlog_json_stream_flush(&chunk->output);
}

bool
tlog_json_chunk_cut(struct tlog_json_chunk *chunk)
{
    tlog_trx_state trx = TLOG_TRX_STATE_ROOT;
    TLOG_TRX_FRAME_DEF_SINGLE(chunk);

    assert(tlog_json_chunk_is_valid(chunk));

    TLOG_TRX_FRAME_BEGIN(trx);

    /* Cut the streams */
    if (!tlog_json_stream_cut(trx, &chunk->input) ||
        !tlog_json_stream_cut(trx, &chunk->output)) {
        TLOG_TRX_FRAME_ABORT(trx);
        return false;
    }

    TLOG_TRX_FRAME_COMMIT(trx);
    return true;
}

void
tlog_json_chunk_empty(struct tlog_json_chunk *chunk)
{
    assert(tlog_json_chunk_is_valid(chunk));
    chunk->rem = chunk->size;
    chunk->timing_ptr = chunk->timing_buf;
    tlog_json_stream_empty(&chunk->input);
    tlog_json_stream_empty(&chunk->output);
    chunk->got_ts = false;
    chunk->first_ts = TLOG_TIMESPEC_ZERO;
    chunk->last_ts = TLOG_TIMESPEC_ZERO;
    if (chunk->window_state > TLOG_JSON_CHUNK_WINDOW_STATE_KNOWN) {
        chunk->window_state = TLOG_JSON_CHUNK_WINDOW_STATE_KNOWN;
    }
    assert(tlog_json_chunk_is_valid(chunk));
}

void
tlog_json_chunk_cleanup(struct tlog_json_chunk *chunk)
{
    assert(chunk != NULL);
    tlog_json_stream_cleanup(&chunk->input);
    tlog_json_stream_cleanup(&chunk->output);
    free(chunk->timing_buf);
    chunk->timing_buf = NULL;
}
