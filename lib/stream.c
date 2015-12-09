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

#include <errno.h>
#include <stdio.h>
#include <tlog/misc.h>
#include <tlog/stream.h>

void
tlog_stream_cleanup(struct tlog_stream *stream)
{
    assert(stream != NULL);
    free(stream->txt_buf);
    stream->txt_buf = NULL;
    free(stream->bin_buf);
    stream->bin_buf = NULL;
}

bool
tlog_stream_is_valid(const struct tlog_stream *stream)
{
    return stream != NULL &&
           stream->size >= TLOG_STREAM_SIZE_MIN &&
           tlog_utf8_is_valid(&stream->utf8) &&
           stream->valid_mark != stream->invalid_mark &&
           stream->txt_buf != NULL &&
           stream->bin_buf != NULL &&
           (stream->txt_len + stream->bin_len) <= stream->size &&
           stream->txt_run <= stream->txt_len &&
           stream->bin_run <= stream->bin_len &&
           (stream->bin_run == 0 || stream->txt_run != 0);
}

bool
tlog_stream_is_pending(const struct tlog_stream *stream)
{
    assert(tlog_stream_is_valid(stream));
    assert(!tlog_utf8_is_ended(&stream->utf8));
    return tlog_utf8_is_started(&stream->utf8);
}

bool
tlog_stream_is_empty(const struct tlog_stream *stream)
{
    assert(tlog_stream_is_valid(stream));
    return stream->txt_len == 0 && stream->bin_len == 0;
}

tlog_grc
tlog_stream_init(struct tlog_stream *stream,
                 struct tlog_dispatcher *dispatcher,
                 size_t size, uint8_t valid_mark, uint8_t invalid_mark)
{
    tlog_grc grc;
    assert(stream != NULL);
    assert(tlog_dispatcher_is_valid(dispatcher));
    assert(size >= TLOG_STREAM_SIZE_MIN);
    assert(valid_mark != invalid_mark);

    memset(stream, 0, sizeof(*stream));

    stream->dispatcher = dispatcher;
    stream->size = size;
    stream->valid_mark = valid_mark;
    stream->invalid_mark = invalid_mark;

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

    assert(tlog_stream_is_valid(stream));
    return TLOG_RC_OK;
error:
    tlog_stream_cleanup(stream);
    return grc;
}

/**
 * Print a byte to a buffer in decimal, as much as it fits.
 *
 * @param buf   The buffer pointer.
 * @param len   The buffer length.
 * @param b     The byte to print.
 *
 * @return Length of the number (to be) printed.
 */
size_t
tlog_stream_btoa(uint8_t *buf, size_t len, uint8_t b)
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
    if (len > 0)
        *buf = '0' + b;
    l++;
    return l;
}

#define REQ(_l) \
    do {                                                \
        if (!tlog_dispatcher_reserve(dispatcher, _l))   \
            return false;                               \
    } while (0)

#define ADV(_l) \
    do {                \
        REQ(_l);        \
        olen += (_l); \
    } while (0)

/**
 * Encode an invalid UTF-8 byte sequence into a JSON array buffer atomically.
 * Reserve space for adding input length in decimal bytes.
 *
 * @param dispatcher    The dispatcher to use.
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
bool
tlog_stream_enc_bin(struct tlog_dispatcher *dispatcher,
                    uint8_t *obuf, size_t *polen,
                    size_t *pirun, size_t *pidig,
                    const uint8_t *ibuf, size_t ilen)
{
    size_t olen;
    size_t irun;
    size_t idig;
    size_t l;

    assert(obuf != NULL);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0)
        return true;

    olen = *polen;
    irun = *pirun;
    idig = *pidig;

    /* If this is the start of a run */
    if (irun == 0) {
        idig = 10;
        /* Reserve space for the marker and single digit run */
        REQ(2);
    }

    for (; ilen > 0; ilen--) {
        irun++;
        if (irun >= idig) {
            REQ(1);
            idig *= 10;
        }
        if (olen > 0) {
            ADV(1);
            *obuf++ = ',';
        }
        l = tlog_stream_btoa(NULL, 0, *ibuf);
        ADV(l);
        tlog_stream_btoa(obuf, l, *ibuf);
        ibuf++;
        obuf += l;
    }

    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    return true;
}

/**
 * Encode a valid UTF-8 byte sequence into a JSON string buffer atomically.
 * Reserve space for adding input length in decimal characters.
 *
 * @param dispatcher    The dispatcher to use.
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
bool
tlog_stream_enc_txt(struct tlog_dispatcher *dispatcher,
                    uint8_t *obuf, size_t *polen,
                    size_t *pirun, size_t *pidig,
                    const uint8_t *ibuf, size_t ilen)
{
    uint8_t c;
    size_t olen;
    size_t irun;
    size_t idig;

    assert(obuf != NULL);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0)
        return true;

    olen = *polen;
    irun = *pirun;
    idig = *pidig;

    /* If this is the start of a run */
    if (irun == 0) {
        idig = 10;
        /* Reserve space for the marker and single digit run */
        REQ(2);
    }

    irun++;
    if (irun >= idig) {
        REQ(1);
        idig *= 10;
    }

    if (ilen > 1) {
        ADV(ilen);
        memcpy(obuf, ibuf, ilen);
    } else {
        c = *ibuf;
        switch (c) {
        case '"':
        case '\\':
            ADV(2);
            *obuf++ = '\\';
            *obuf = c;
            break;
#define ESC_CASE(_c, _e) \
        case _c:            \
            ADV(2);        \
            *obuf++ = '\\'; \
            *obuf = _e;   \
            break;
        ESC_CASE('\b', 'b');
        ESC_CASE('\f', 'f');
        ESC_CASE('\n', 'n');
        ESC_CASE('\r', 'r');
        ESC_CASE('\t', 't');
#undef ESC
        default:
            if (c < 0x20 || c == 0x7f) {
                ADV(6);
                *obuf++ = '\\';
                *obuf++ = 'u';
                *obuf++ = '0';
                *obuf++ = '0';
                *obuf++ = tlog_nibble_digit(c >> 4);
                *obuf = tlog_nibble_digit(c & 0xf);
                break;
            } else {
                ADV(1);
                *obuf = c;
            }
            break;
        }
    }

    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    return true;
}

#undef ADV
#undef REQ

/**
 * Write a meta record for text and binary runs, resetting them.
 *
 * @param valid_mark    Valid UTF-8 record marker character.
 * @param invalid_mark  Invalid UTF-8 record marker character.
 * @param ptxt_run      Location of/for text run.
 * @param pbin_run      Location of/for binary run.
 * @param pmeta         Location of/for the meta data output pointer.
 */
static void
tlog_stream_write_meta(struct tlog_dispatcher *dispatcher,
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
        tlog_dispatcher_write(dispatcher, (uint8_t *)buf, rc);
        *ptxt_run = 0;
    }
    if (*pbin_run != 0) {
        rc = snprintf(buf, sizeof(buf), "/%zu", *pbin_run);
        assert(rc >= 0);
        assert(rc < (int)sizeof(buf));
        tlog_dispatcher_write(dispatcher, (uint8_t *)buf, rc);
        *pbin_run = 0;
    }
}

void
tlog_stream_flush(struct tlog_stream *stream)
{
    assert(tlog_stream_is_valid(stream));
    tlog_stream_write_meta(stream->dispatcher,
                           stream->valid_mark, stream->invalid_mark,
                           &stream->txt_run, &stream->bin_run);
}

/**
 * Atomically write a byte sequence to a stream, the sequence being either a
 * valid or an invalid UTF-8 character. Account for any (potentially) used
 * total remaining space.
 *
 * @param stream    The stream to write to.    
 * @param ts        The byte sequence timestamp.
 * @param valid     True if sequence is a valid character, false otherwise.
 * @param buf       Sequence buffer pointer.
 * @param len       Sequence buffer length.
 *
 * @return True if the sequence was written, false otherwise.
 */
static bool
tlog_stream_write_seq(struct tlog_stream *stream, const struct timespec *ts,
                      bool valid, const uint8_t *buf, size_t len)
{
    /* Unicode replacement character (u+fffd) */
    static const uint8_t repl_buf[] = {0xef, 0xbf, 0xbd};
    size_t txt_run;
    size_t txt_dig;
    size_t txt_len;
    size_t bin_run;
    size_t bin_dig;
    size_t bin_len;

    assert(tlog_stream_is_valid(stream));
    assert(buf != NULL || len == 0);

    if (len == 0)
        return true;
    
    txt_run = stream->txt_run;
    txt_dig = stream->txt_dig;
    txt_len = stream->txt_len;
    bin_run = stream->bin_run;
    bin_dig = stream->bin_dig;
    bin_len = stream->bin_len;

    /* Cut the run, if changing type */
    if ((!valid) != (stream->bin_run != 0))
        tlog_stream_write_meta(stream->dispatcher,
                               stream->valid_mark, stream->invalid_mark,
                               &txt_run, &bin_run);

    /* Advance the time */
    tlog_dispatcher_advance(stream->dispatcher, ts);

    if (valid) {
        /* Write the character to the text buffer */
        if (!tlog_stream_enc_txt(stream->dispatcher,
                                 stream->txt_buf + txt_len, &txt_len,
                                 &txt_run, &txt_dig,
                                 buf, len))
            return false;
    } else {
        /* Write the replacement character to the text buffer */
        if (!tlog_stream_enc_txt(stream->dispatcher,
                                 stream->txt_buf + txt_len,  &txt_len,
                                 &txt_run, &txt_dig,
                                 repl_buf, sizeof(repl_buf)))
            return false;

        /* Write bytes to the binary buffer */
        if (!tlog_stream_enc_bin(stream->dispatcher,
                                 stream->bin_buf + bin_len, &bin_len,
                                 &bin_run, &bin_dig,
                                 buf, len))
            return false;
    }

    stream->txt_run = txt_run;
    stream->txt_dig = txt_dig;
    stream->txt_len = txt_len;
    stream->bin_run = bin_run;
    stream->bin_dig = bin_dig;
    stream->bin_len = bin_len;
    return true;
}


size_t
tlog_stream_write(struct tlog_stream *stream,
                  const struct timespec *ts,
                  const uint8_t **pbuf, size_t *plen)
{
    const uint8_t *buf;
    size_t len;
    struct tlog_utf8 *utf8;
    size_t written;

    assert(tlog_stream_is_valid(stream));
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);

    buf = *pbuf;
    len = *plen;
    utf8 = &stream->utf8;
    assert(!tlog_utf8_is_ended(utf8));

    while (true) {
        do {
            /* If input is exhausted */
            if (len == 0) {
                /* Exit but leave the incomplete character buffered */
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
            /* Write single input byte as invalid sequence and skip it */
            if (!tlog_stream_write_seq(stream, &stream->ts, false, buf, 1))
                break;
            buf++;
            len--;
        } else {
            /* If the (in)complete character doesn't fit into output */
            if (!tlog_stream_write_seq(stream, &stream->ts,
                                       tlog_utf8_is_complete(utf8),
                                       utf8->buf, utf8->len)) {
                /* Back up unwritten data */
                buf -= utf8->len;
                len += utf8->len;
                break;
            }
        }
        tlog_utf8_reset(utf8);
    }

    tlog_utf8_reset(utf8);
exit:
    written = (*plen - len);
    *pbuf = buf;
    *plen = len;
    return written;
}

bool
tlog_stream_cut(struct tlog_stream *stream)
{
    struct tlog_utf8 *utf8;

    assert(tlog_stream_is_valid(stream));

    utf8 = &stream->utf8;
    assert(!tlog_utf8_is_ended(utf8));

    if (tlog_stream_write_seq(stream, &stream->ts,
                              false, utf8->buf, utf8->len)) {
        tlog_utf8_reset(utf8);
        return true;
    } else {
        return false;
    }
}

void
tlog_stream_empty(struct tlog_stream *stream)
{
    assert(tlog_stream_is_valid(stream));
    stream->txt_run = 0;
    stream->txt_len = 0;
    stream->bin_run = 0;
    stream->bin_len = 0;
}

/** Stream transaction store */
TLOG_TRX_STORE_SIG(tlog_stream) {
    TLOG_TRX_STORE_PROXY(dispatcher);
    TLOG_TRX_STORE_VAR(tlog_stream, utf8);
    TLOG_TRX_STORE_VAR(tlog_stream, ts);
    TLOG_TRX_STORE_VAR(tlog_stream, txt_run);
    TLOG_TRX_STORE_VAR(tlog_stream, txt_dig);
    TLOG_TRX_STORE_VAR(tlog_stream, txt_len);
    TLOG_TRX_STORE_VAR(tlog_stream, bin_run);
    TLOG_TRX_STORE_VAR(tlog_stream, bin_dig);
    TLOG_TRX_STORE_VAR(tlog_stream, bin_len);
};

/** Transfer transaction data of a stream */
static TLOG_TRX_XFR_SIG(tlog_stream)
{
    TLOG_TRX_XFR_PROLOGUE(tlog_stream);

    TLOG_TRX_XFR_PROXY(dispatcher);
    TLOG_TRX_XFR_VAR(utf8);
    TLOG_TRX_XFR_VAR(ts);
    TLOG_TRX_XFR_VAR(txt_run);
    TLOG_TRX_XFR_VAR(txt_dig);
    TLOG_TRX_XFR_VAR(txt_len);
    TLOG_TRX_XFR_VAR(bin_run);
    TLOG_TRX_XFR_VAR(bin_dig);
    TLOG_TRX_XFR_VAR(bin_len);

    TLOG_TRX_XFR_EPILOGUE;
}

struct tlog_trx_iface TLOG_TRX_IFACE_NAME(tlog_stream) = {
    .store_size = sizeof(TLOG_TRX_STORE_SIG(tlog_stream)),
    .mask_off = TLOG_OFFSET_OF(struct tlog_stream, trx_mask),
    .xfr = TLOG_TRX_XFR_NAME(tlog_stream)
};
