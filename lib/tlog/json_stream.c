/*
 * JSON-encoded terminal I/O stream.
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
#include <tlog/misc.h>
#include <tlog/json_stream.h>
#include <tlog/rc.h>

void
tlog_json_stream_cleanup(struct tlog_json_stream *stream)
{
    assert(stream != NULL);
    free(stream->txt_buf);
    stream->txt_buf = NULL;
    free(stream->bin_buf);
    stream->bin_buf = NULL;
}

bool
tlog_json_stream_is_valid(const struct tlog_json_stream *stream)
{
    return stream != NULL &&
           stream->size >= TLOG_JSON_STREAM_SIZE_MIN &&
           tlog_utf8_is_valid(&stream->utf8) &&
           stream->valid_mark != stream->invalid_mark &&
           stream->txt_buf != NULL &&
           stream->bin_buf != NULL &&
           (stream->txt_len + stream->bin_len) <= stream->size &&
           stream->txt_run <= stream->txt_len &&
           stream->bin_run <= stream->bin_len &&
           /* Text buffer cannot be empty if binary is not */
           (stream->bin_run == 0 || stream->txt_run != 0);
}

bool
tlog_json_stream_is_pending(const struct tlog_json_stream *stream)
{
    assert(tlog_json_stream_is_valid(stream));
    assert(!tlog_utf8_is_ended(&stream->utf8));
    return tlog_utf8_is_started(&stream->utf8);
}

bool
tlog_json_stream_is_empty(const struct tlog_json_stream *stream)
{
    assert(tlog_json_stream_is_valid(stream));
    return stream->txt_len == 0 && stream->bin_len == 0;
}

static
TLOG_TRX_BASIC_ACT_SIG(tlog_json_stream)
{
    TLOG_TRX_BASIC_ACT_PROLOGUE(tlog_json_stream);
    TLOG_TRX_BASIC_ACT_ON_VAR(utf8);
    TLOG_TRX_BASIC_ACT_ON_VAR(ts);
    TLOG_TRX_BASIC_ACT_ON_VAR(txt_run);
    TLOG_TRX_BASIC_ACT_ON_VAR(txt_dig);
    TLOG_TRX_BASIC_ACT_ON_VAR(txt_len);
    TLOG_TRX_BASIC_ACT_ON_VAR(bin_run);
    TLOG_TRX_BASIC_ACT_ON_VAR(bin_dig);
    TLOG_TRX_BASIC_ACT_ON_VAR(bin_len);
    TLOG_TRX_BASIC_ACT_ON_PTR(dispatcher);
}

tlog_grc
tlog_json_stream_init(struct tlog_json_stream *stream,
                      struct tlog_json_dispatcher *dispatcher,
                      size_t size, uint8_t valid_mark, uint8_t invalid_mark)
{
    tlog_grc grc;
    assert(stream != NULL);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(size >= TLOG_JSON_STREAM_SIZE_MIN);
    assert(valid_mark != invalid_mark);

    memset(stream, 0, sizeof(*stream));

    stream->dispatcher = dispatcher;
    stream->size = size;
    stream->valid_mark = valid_mark;
    stream->invalid_mark = invalid_mark;
    stream->trx_iface = TLOG_TRX_BASIC_IFACE(tlog_json_stream);

    stream->txt_buf = malloc(size);
    if (stream->txt_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    stream->bin_buf = malloc(size);
    if (stream->bin_buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    assert(tlog_json_stream_is_valid(stream));
    return TLOG_RC_OK;
error:
    tlog_json_stream_cleanup(stream);
    return grc;
}

size_t
tlog_json_stream_btoa(uint8_t *buf, size_t len, uint8_t b)
{
    size_t l = 0;
    if (b >= 100) {
        if (len > 0) {
            len--;
            *buf++ = '0' + b/100;
        }
        b %= 100;
        l++;
    }
    if (b >= 10) {
        if (len > 0) {
            len--;
            *buf++ = '0' + b/10;
        }
        b %= 10;
        l++;
    } else if (l > 0) {
        if (len > 0) {
            len--;
            *buf++ = '0';
        }
        l++;
    }
    if (len > 0) {
        *buf = '0' + b;
    }
    l++;
    return l;
}

#define REQ(_dispatcher, _l) \
    do {                                                      \
        if (!tlog_json_dispatcher_reserve(_dispatcher, _l)) { \
            goto failure;                                     \
        }                                                     \
    } while (0)

#define ADV(_dispatcher, _l) \
    do {                        \
        REQ(_dispatcher, _l);   \
        olen += (_l);           \
    } while (0)

bool
tlog_json_stream_enc_bin(tlog_trx_state trx,
                         struct tlog_json_dispatcher *dispatcher,
                         uint8_t *obuf, size_t *polen,
                         size_t *pirun, size_t *pidig,
                         const uint8_t *ibuf, size_t ilen)
{
    size_t olen;
    size_t irun;
    size_t idig;
    size_t l;
    TLOG_TRX_FRAME_DEF_SINGLE(dispatcher);

    assert(obuf != NULL);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0) {
        return true;
    }

    TLOG_TRX_FRAME_BEGIN(trx);
    olen = *polen;
    irun = *pirun;
    idig = *pidig;

    /* If this is the start of a run */
    if (irun == 0) {
        idig = 10;
        /* Reserve space for the marker and single digit run */
        REQ(dispatcher, 2);
    }

    for (; ilen > 0; ilen--) {
        irun++;
        if (irun >= idig) {
            REQ(dispatcher, 1);
            idig *= 10;
        }
        if (olen > 0) {
            ADV(dispatcher, 1);
            *obuf++ = ',';
        }
        l = tlog_json_stream_btoa(NULL, 0, *ibuf);
        ADV(dispatcher, l);
        tlog_json_stream_btoa(obuf, l, *ibuf);
        ibuf++;
        obuf += l;
    }

    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    TLOG_TRX_FRAME_COMMIT(trx);
    return true;
failure:
    TLOG_TRX_FRAME_ABORT(trx);
    return false;
}

bool
tlog_json_stream_enc_txt(tlog_trx_state trx,
                         struct tlog_json_dispatcher *dispatcher,
                         uint8_t *obuf, size_t *polen,
                         size_t *pirun, size_t *pidig,
                         const uint8_t *ibuf, size_t ilen)
{
    uint8_t c;
    size_t olen;
    size_t irun;
    size_t idig;
    TLOG_TRX_FRAME_DEF_SINGLE(dispatcher);

    assert(obuf != NULL);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0) {
        return true;
    }

    TLOG_TRX_FRAME_BEGIN(trx);
    olen = *polen;
    irun = *pirun;
    idig = *pidig;

    /* If this is the start of a run */
    if (irun == 0) {
        idig = 10;
        /* Reserve space for the marker and single digit run */
        REQ(dispatcher, 2);
    }

    irun++;
    if (irun >= idig) {
        REQ(dispatcher, 1);
        idig *= 10;
    }

    if (ilen > 1) {
        ADV(dispatcher, ilen);
        memcpy(obuf, ibuf, ilen);
    } else {
        c = *ibuf;
        switch (c) {
        case '"':
        case '\\':
            ADV(dispatcher, 2);
            *obuf++ = '\\';
            *obuf = c;
            break;
#define ESC_CASE(_c, _e) \
        case _c:                \
            ADV(dispatcher, 2); \
            *obuf++ = '\\';     \
            *obuf = _e;         \
            break;
        ESC_CASE('\b', 'b');
        ESC_CASE('\f', 'f');
        ESC_CASE('\n', 'n');
        ESC_CASE('\r', 'r');
        ESC_CASE('\t', 't');
#undef ESC
        default:
            if (c < 0x20 || c == 0x7f) {
                ADV(dispatcher, 6);
                *obuf++ = '\\';
                *obuf++ = 'u';
                *obuf++ = '0';
                *obuf++ = '0';
                *obuf++ = tlog_nibble_digit(c >> 4);
                *obuf = tlog_nibble_digit(c & 0xf);
                break;
            } else {
                ADV(dispatcher, 1);
                *obuf = c;
            }
            break;
        }
    }

    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    TLOG_TRX_FRAME_COMMIT(trx);
    return true;
failure:
    TLOG_TRX_FRAME_ABORT(trx);
    return false;
}

#undef ADV
#undef REQ

/**
 * Write a meta record for text and binary runs, resetting them.
 *
 * @param dispatcher    The dispatcher to write through.
 * @param valid_mark    Valid UTF-8 record marker character.
 * @param invalid_mark  Invalid UTF-8 record marker character.
 * @param ptxt_run      Location of/for text run.
 * @param pbin_run      Location of/for binary run.
 * @param pmeta         Location of/for the meta data output pointer.
 */
static void
tlog_json_stream_write_meta(struct tlog_json_dispatcher *dispatcher,
                            uint8_t valid_mark, uint8_t invalid_mark,
                            size_t *ptxt_run, size_t *pbin_run)
{
    int rc;
    char buf[32];

    assert(valid_mark != invalid_mark);
    assert(ptxt_run != NULL);
    assert(pbin_run != NULL);

    if (*ptxt_run != 0) {
        rc = snprintf(buf, sizeof(buf), "%c%zu",
                      (*pbin_run == 0 ? valid_mark
                                            : invalid_mark),
                      *ptxt_run);
        assert(rc >= 0);
        assert(rc < (int)sizeof(buf));
        tlog_json_dispatcher_write(dispatcher, (uint8_t *)buf, rc);
    }
    if (*pbin_run != 0) {
        rc = snprintf(buf, sizeof(buf), "/%zu", *pbin_run);
        assert(rc >= 0);
        assert(rc < (int)sizeof(buf));
        tlog_json_dispatcher_write(dispatcher, (uint8_t *)buf, rc);
    }
    *ptxt_run = 0;
    *pbin_run = 0;
}

void
tlog_json_stream_flush(struct tlog_json_stream *stream)
{
    assert(tlog_json_stream_is_valid(stream));
    tlog_json_stream_write_meta(stream->dispatcher,
                                stream->valid_mark, stream->invalid_mark,
                                &stream->txt_run, &stream->bin_run);
}

/**
 * Atomically write a byte sequence to a stream, the sequence being either a
 * valid or an invalid UTF-8 character. Account for any (potentially) used
 * total remaining space.
 *
 * @param trx       Transaction to act within.
 * @param stream    The stream to write to.    
 * @param ts        The byte sequence timestamp.
 * @param valid     True if sequence is a valid character, false otherwise.
 * @param buf       Sequence buffer pointer.
 * @param len       Sequence buffer length.
 *
 * @return True if the sequence was written, false otherwise.
 */
static bool
tlog_json_stream_write_seq(tlog_trx_state trx,
                           struct tlog_json_stream *stream,
                           const struct timespec *ts,
                           bool valid, const uint8_t *buf, size_t len)
{
    /* Unicode replacement character (u+fffd) */
    static const uint8_t repl_buf[] = {0xef, 0xbf, 0xbd};
    TLOG_TRX_FRAME_DEF_SINGLE(stream);

    assert(tlog_json_stream_is_valid(stream));
    assert(buf != NULL || len == 0);

    if (len == 0) {
        return true;
    }

    TLOG_TRX_FRAME_BEGIN(trx);

    /* Cut the run, if changing type */
    if ((!valid) != (stream->bin_run != 0)) {
        tlog_json_stream_write_meta(stream->dispatcher,
                                    stream->valid_mark, stream->invalid_mark,
                                    &stream->txt_run, &stream->bin_run);
    }

    /* Advance the time */
    if (!tlog_json_dispatcher_advance(trx, stream->dispatcher, ts)) {
        goto failure;
    }

    if (valid) {
        /* Write the character to the text buffer */
        if (!tlog_json_stream_enc_txt(trx,
                                      stream->dispatcher,
                                      stream->txt_buf + stream->txt_len,
                                      &stream->txt_len,
                                      &stream->txt_run, &stream->txt_dig,
                                      buf, len)) {
            goto failure;
        }
    } else {
        /* Write the replacement character to the text buffer */
        if (!tlog_json_stream_enc_txt(trx,
                                      stream->dispatcher,
                                      stream->txt_buf + stream->txt_len,
                                      &stream->txt_len,
                                      &stream->txt_run, &stream->txt_dig,
                                      repl_buf, sizeof(repl_buf))) {
            goto failure;
        }

        /* Write bytes to the binary buffer */
        if (!tlog_json_stream_enc_bin(trx,
                                      stream->dispatcher,
                                      stream->bin_buf + stream->bin_len,
                                      &stream->bin_len,
                                      &stream->bin_run, &stream->bin_dig,
                                      buf, len)) {
            goto failure;
        }
    }

    TLOG_TRX_FRAME_COMMIT(trx);
    return true;

failure:
    TLOG_TRX_FRAME_ABORT(trx);
    return false;
}


size_t
tlog_json_stream_write(tlog_trx_state trx,
                       struct tlog_json_stream *stream,
                       const struct timespec *ts,
                       const uint8_t **pbuf, size_t *plen)
{
    const uint8_t *buf;
    size_t len;
    struct tlog_utf8 *utf8;
    size_t written;
    TLOG_TRX_FRAME_DEF_SINGLE(stream);

    assert(tlog_json_stream_is_valid(stream));
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);

    buf = *pbuf;
    len = *plen;
    utf8 = &stream->utf8;
    assert(!tlog_utf8_is_ended(utf8));

    /*
     * While there's input and byte sequences fit
     */
    trx = TLOG_TRX_STATE_SUB(trx);
    while (true) {
        const uint8_t *start_buf = buf;
        size_t start_len = len;

        TLOG_TRX_FRAME_BEGIN(trx);

        /*
         * Until the current UTF-8 sequence is ended and is considered either
         * complete or incomplete, or the input is exhausted
         */
        do {
            /* If input is exhausted */
            if (len == 0) {
                /* Exit but leave the incomplete character buffered */
                TLOG_TRX_FRAME_COMMIT(trx);
                goto exit;
            }
            /* If the byte was added */
            if (tlog_utf8_add(utf8, *buf)) {
                buf++;
                len--;
                /* Record last added byte timestamp */
                stream->ts = *ts;
            }
        } while (!tlog_utf8_is_ended(utf8));

        /* If the first byte we encountered was invalid */
        if (tlog_utf8_is_empty(utf8)) {
            /* Write the single input byte as invalid sequence and skip it */
            if (!tlog_json_stream_write_seq(trx, stream, ts, false, buf, 1)) {
                TLOG_TRX_FRAME_ABORT(trx);
                goto exit;
            }
            buf++;
            len--;
        } else {
            /* If the (in)complete character doesn't fit into output */
            if (!tlog_json_stream_write_seq(trx, stream, &stream->ts,
                                            tlog_utf8_is_complete(utf8),
                                            utf8->buf, utf8->len)) {
                /* Back up unwritten data */
                buf = start_buf;
                len = start_len;
                TLOG_TRX_FRAME_ABORT(trx);
                goto exit;
            }
        }
        tlog_utf8_reset(utf8);
        TLOG_TRX_FRAME_COMMIT(trx);
    }

exit:

    written = (*plen - len);
    *pbuf = buf;
    *plen = len;

    return written;
}

bool
tlog_json_stream_cut(tlog_trx_state trx,
                     struct tlog_json_stream *stream)
{
    struct tlog_utf8 *utf8;
    TLOG_TRX_FRAME_DEF_SINGLE(stream);

    assert(tlog_json_stream_is_valid(stream));

    utf8 = &stream->utf8;
    assert(!tlog_utf8_is_ended(utf8));

    TLOG_TRX_FRAME_BEGIN(trx);
    if (tlog_json_stream_write_seq(trx, stream, &stream->ts,
                                   false, utf8->buf, utf8->len)) {
        tlog_utf8_reset(utf8);
        TLOG_TRX_FRAME_COMMIT(trx);
        return true;
    } else {
        TLOG_TRX_FRAME_ABORT(trx);
        return false;
    }
}

void
tlog_json_stream_empty(struct tlog_json_stream *stream)
{
    assert(tlog_json_stream_is_valid(stream));
    stream->txt_run = 0;
    stream->txt_len = 0;
    stream->bin_run = 0;
    stream->bin_len = 0;
}
