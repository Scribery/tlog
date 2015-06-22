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
#include "tlog_misc.h"
#include "tlog_stream.h"

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
           stream->txt_run < stream->txt_len &&
           stream->bin_run < stream->bin_len;
}

tlog_rc
tlog_stream_init(struct tlog_stream *stream, size_t size,
                 uint8_t valid_mark, uint8_t invalid_mark)
{
    int orig_errno;
    assert(stream != NULL);
    assert(size >= TLOG_STREAM_SIZE_MIN);
    assert(valid_mark != invalid_mark);

    memset(stream, 0, sizeof(*stream));

    stream->size = size;

    stream->valid_mark = valid_mark;
    stream->invalid_mark = invalid_mark;

    stream->txt_buf = malloc(size);
    if (stream->txt_buf == NULL)
        goto error;

    stream->bin_buf = malloc(size);
    if (stream->bin_buf == NULL)
        goto error;

    return TLOG_RC_OK;
error:
    orig_errno = errno;
    tlog_stream_cleanup(stream);
    errno = orig_errno;
    return TLOG_RC_FAILURE;
}

/**
 * Print a byte to a buffer in decimal, as much as it fits.
 *
 * @param buf   The buffer pointer.
 * @param len   The buffer length.
 * @param b     The byte to print.
 *
 * @return Length of the (to be) printed number.
 */
static size_t
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
    do {                    \
        if ((_l) > orem)  \
            return false;   \
        orem -= (_l);     \
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
 * @param obuf  Pointer to the output buffer.
 * @param porem Location of/for the remaining output space, in bytes.
 * @param polen Location of/for output byte counter.
 * @param pirun Location of/for input character counter.
 * @param pidig Location of/for the next digit input counter limit.
 * @param ibuf  Pointer to the input buffer.
 * @param ilen  Pointer to the input length.
 *
 * @return True if both the bytes and the new counter did fit into the
 *         remaining output space.
 */
static bool
tlog_stream_enc_bin(uint8_t *obuf, size_t *porem, size_t *polen,
                    size_t *pirun, size_t *pidig,
                    const uint8_t *ibuf, size_t ilen)
{
    size_t orem;
    size_t olen;
    size_t irun;
    size_t idig;
    size_t l;

    assert(porem != NULL);
    assert(obuf != NULL || *porem == 0);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0)
        return true;

    orem = *porem;
    olen = *polen;
    irun = *pirun;
    idig = *pidig;

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
        l = tlog_stream_btoa(obuf, orem, *ibuf++);
        ADV(l);
        obuf += l;
    }

    *porem = orem;
    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    return true;
}

/**
 * Encode a valid UTF-8 byte sequence into a JSON string buffer atomically.
 * Reserve space for adding input length in decimal characters.
 *
 * @param obuf  Pointer to the output buffer.
 * @param porem Location of/for the remaining output space, in bytes.
 * @param polen Location of/for output byte counter.
 * @param pirun Location of/for input character counter.
 * @param pidig Location of/for the next digit input counter limit.
 * @param ibuf  Pointer to the input buffer.
 * @param ilen  Pointer to the input length.
 *
 * @return True if both the character and the new counter did fit into the
 *         remaining output space.
 */
static bool
tlog_stream_enc_txt(uint8_t *obuf, size_t *porem, size_t *polen,
                    size_t *pirun, size_t *pidig,
                    const uint8_t *ibuf, size_t ilen)
{
    uint8_t c;
    size_t orem;
    size_t olen;
    size_t irun;
    size_t idig;

    assert(porem != NULL);
    assert(obuf != NULL || *porem == 0);
    assert(polen != NULL);
    assert(pirun != NULL);
    assert(pidig != NULL);
    assert(ibuf != NULL || ilen == 0);

    if (ilen == 0)
        return true;

    orem = *porem;
    olen = *polen;
    irun = *pirun;
    idig = *pidig;

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

    *porem = orem;
    *polen = olen;
    *pirun = irun;
    *pidig = idig;
    return true;
}

#undef ADV
#undef REQ

/**
 * Atomically write a byte sequence to a stream, the sequence being either a
 * valid or an invalid UTF-8 character. Account for any (potentially) used
 * total remaining space.
 *
 * @param stream    The stream to write to.    
 * @param invalid   True if sequence is an invalid character, false otherwise.
 * @param buf       Sequence buffer pointer.
 * @param len       Sequence buffer length.
 * @param pmeta     Location of/for the meta data output pointer.
 * @param prem      Location of/for the total remaining output space.
 *
 * @return True if the sequence was written, false otherwise.
 */
static bool
tlog_stream_write_seq(struct tlog_stream *stream, bool invalid,
                      uint8_t *buf, size_t len,
                      uint8_t **pmeta, size_t *prem)
{
    /* Unicode replacement character (u+fffd) */
    static const uint8_t repl_buf[] = {0xef, 0xbf, 0xbd};
    uint8_t *meta;
    size_t rem;
    size_t txt_run;
    size_t txt_dig;
    size_t txt_len;
    size_t bin_run;
    size_t bin_dig;
    size_t bin_len;
    int rc;
    bool complete;

    assert(tlog_stream_is_valid(stream));
    assert(buf != NULL || len == 0);
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    assert(prem != NULL);

    if (len == 0)
        return true;

    complete = false;
    txt_run = stream->txt_run;
    txt_dig = stream->txt_dig;
    txt_len = stream->txt_len;
    bin_run = stream->bin_run;
    bin_dig = stream->bin_dig;
    bin_len = stream->bin_len;
    meta = *pmeta;
    rem = *prem;

#define REQ(_l) \
    do {                    \
        if ((_l) > rem)     \
            return false;   \
        rem -= (_l);        \
    } while (0)

#define ADV(_l) \
    do {                \
        REQ(_l);        \
        meta += (_l);   \
    } while (0)

    /* If changing run type */
    if (invalid != stream->invalid) {
        if (stream->invalid) {
            /* Write meta record for invalid run */
            if (stream->txt_run != 0 || stream->bin_run != 0) {
                rc = snprintf((char *)meta, rem, "%c%zu/%zu",
                              stream->invalid_mark,
                              stream->txt_run, stream->bin_run);
                assert(rc >= 0);
                ADV((size_t)rc);
            }
            /* Reset run counters */
            txt_run = 0;
            txt_dig = 10;
            bin_run = 0;
            bin_dig = 10;
            /*
             * Account for the minimal valid meta record
             * (mark and one digit)
             */
            REQ(2);
        } else {
            /* Write meta record for valid run */
            if (stream->txt_run != 0) {
                rc = snprintf((char *)meta, rem, "%c%zu",
                              stream->valid_mark, stream->txt_run);
                assert(rc >= 0);
                ADV((size_t)rc);
            }
            /* Reset run counters */
            txt_run = 0;
            txt_dig = 10;
            /*
             * Account for the minimal invalid meta record
             * (mark, two digits, and separator in between)
             */
            REQ(4);
        }
    }

#undef ADV
#undef REQ

    if (invalid) {
        /* Write the replacement character to the text buffer */
        if (!tlog_stream_enc_txt(stream->txt_buf + txt_len, &rem, &txt_len,
                                 &txt_run, &txt_dig,
                                 repl_buf, sizeof(repl_buf)))
            goto exit;

        /* Write bytes to the binary buffer */
        if (!tlog_stream_enc_bin(stream->bin_buf + bin_len, &rem,
                                 &bin_run, &bin_dig, &bin_len,
                                 buf, len))
            goto exit;
    } else {
        /* Write the character to the text buffer */
        if (!tlog_stream_enc_txt(stream->txt_buf + txt_len, &rem,
                                 &txt_run, &txt_dig, &txt_len,
                                 buf, len))
            goto exit;
    }

    complete = true;
    stream->invalid = invalid;
    stream->txt_run = txt_run;
    stream->txt_dig = txt_dig;
    stream->txt_len = txt_len;
    stream->bin_run = bin_run;
    stream->bin_dig = bin_dig;
    stream->bin_len = bin_len;
    *prem = rem;
    *pmeta = meta;
exit:
    return complete;
}


void
tlog_stream_write(struct tlog_stream *stream,
                  uint8_t **pbuf, size_t *plen,
                  uint8_t **pmeta,
                  size_t *prem)
{
    uint8_t *buf;
    size_t len;
    struct tlog_utf8 *utf8;
    bool written;

    assert(tlog_stream_is_valid(stream));
    assert(!tlog_utf8_is_ended(&stream->utf8));
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    assert(prem != NULL);

    buf = *pbuf;
    len = *plen;
    utf8 = &stream->utf8;

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
            }
        } while (!tlog_utf8_is_ended(utf8));

        /* If the first byte we encountered was invalid */
        if (tlog_utf8_is_empty(utf8)) {
            /* Write single input byte as invalid sequence and skip it */
            if (!tlog_stream_write_seq(stream, true, buf, 1, pmeta, prem))
                break;
            buf++;
            len--;
        } else {
            /* If this is a complete UTF-8 character */
            if (tlog_utf8_is_complete(utf8)) {
                /* Write complete utf8 buffer as valid sequence */
                written = tlog_stream_write_seq(stream, false,
                                                utf8->buf, utf8->len,
                                                pmeta, prem);
            /* If this is an incomplete UTF-8 character */
            } else {
                /* Write utf8 buffer as invalid sequence */
                written = tlog_stream_write_seq(stream, true,
                                                utf8->buf, utf8->len,
                                                pmeta, prem);
            }

            /* If the buffer didn't fit into output */
            if (!written) {
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
    *pbuf = buf;
    *plen = len;
}
