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
tlog_stream_init(struct tlog_stream *stream, size_t size,
                 uint8_t valid_mark, uint8_t invalid_mark)
{
    tlog_grc grc;
    assert(stream != NULL);
    assert(size >= TLOG_STREAM_SIZE_MIN);
    assert(valid_mark != invalid_mark);

    memset(stream, 0, sizeof(*stream));

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
 * @return Length of the (to be) printed number.
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
bool
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
bool
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

    *porem = orem;
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
tlog_stream_write_meta(uint8_t valid_mark, uint8_t invalid_mark,
                       size_t *ptxt_run, size_t *pbin_run,
                       uint8_t **pmeta)
{
    int rc;
    char buf[32];
    char *meta;

    assert(valid_mark != invalid_mark);
    assert(ptxt_run != NULL);
    assert(pbin_run != NULL);
    assert(pmeta != NULL);
    assert(*pmeta != NULL);

    meta = (char *)*pmeta;

    if (*ptxt_run != 0) {
        rc = snprintf(buf, sizeof(buf), "%c%zu",
                      (*pbin_run == 0 ? valid_mark
                                            : invalid_mark),
                      *ptxt_run);
        assert(rc >= 0);
        assert(rc < (int)sizeof(buf));
        memcpy(meta, buf, rc);
        meta += rc;
        *ptxt_run = 0;
    }
    if (*pbin_run != 0) {
        rc = snprintf(buf, sizeof(buf), "/%zu", *pbin_run);
        assert(rc >= 0);
        assert(rc < (int)sizeof(buf));
        memcpy(meta, buf, rc);
        meta += rc;
        *pbin_run = 0;
    }

    *pmeta = (uint8_t *)meta;
}

void
tlog_stream_flush(struct tlog_stream *stream, uint8_t **pmeta)
{
    assert(tlog_stream_is_valid(stream));
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    tlog_stream_write_meta(stream->valid_mark, stream->invalid_mark,
                           &stream->txt_run, &stream->bin_run,
                           pmeta);
}

/**
 * Atomically write a byte sequence to a stream, the sequence being either a
 * valid or an invalid UTF-8 character. Account for any (potentially) used
 * total remaining space.
 *
 * @param stream    The stream to write to.    
 * @param valid     True if sequence is a valid character, false otherwise.
 * @param buf       Sequence buffer pointer.
 * @param len       Sequence buffer length.
 * @param pmeta     Location of/for the meta data output pointer.
 * @param prem      Location of/for the total remaining output space.
 *
 * @return True if the sequence was written, false otherwise.
 */
static bool
tlog_stream_write_seq(struct tlog_stream *stream, bool valid,
                      const uint8_t *buf, size_t len,
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

    assert(tlog_stream_is_valid(stream));
    assert(buf != NULL || len == 0);
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    assert(prem != NULL);

    if (len == 0)
        return true;
    
    txt_run = stream->txt_run;
    txt_dig = stream->txt_dig;
    txt_len = stream->txt_len;
    bin_run = stream->bin_run;
    bin_dig = stream->bin_dig;
    bin_len = stream->bin_len;
    meta = *pmeta;
    rem = *prem;

    /* Cut the run, if changing type */
    if ((!valid) != (stream->bin_run != 0))
        tlog_stream_write_meta(stream->valid_mark, stream->invalid_mark,
                               &txt_run, &bin_run, &meta);

    if (valid) {
        /* Write the character to the text buffer */
        if (!tlog_stream_enc_txt(stream->txt_buf + txt_len, &rem, &txt_len,
                                 &txt_run, &txt_dig,
                                 buf, len))
            return false;
    } else {
        /* Write the replacement character to the text buffer */
        if (!tlog_stream_enc_txt(stream->txt_buf + txt_len, &rem, &txt_len,
                                 &txt_run, &txt_dig,
                                 repl_buf, sizeof(repl_buf)))
            return false;

        /* Write bytes to the binary buffer */
        if (!tlog_stream_enc_bin(stream->bin_buf + bin_len, &rem, &bin_len,
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
    *prem = rem;
    *pmeta = meta;
    return true;
}


size_t
tlog_stream_write(struct tlog_stream *stream,
                  const uint8_t **pbuf, size_t *plen,
                  uint8_t **pmeta, size_t *prem)
{
    const uint8_t *buf;
    size_t len;
    struct tlog_utf8 *utf8;
    size_t written;

    assert(tlog_stream_is_valid(stream));
    assert(pbuf != NULL);
    assert(plen != NULL);
    assert(*pbuf != NULL || *plen == 0);
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    assert(prem != NULL);
    assert(*prem <= (stream->size - stream->bin_len - stream->txt_len));

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
            }
        } while (!tlog_utf8_is_ended(utf8));

        /* If the first byte we encountered was invalid */
        if (tlog_utf8_is_empty(utf8)) {
            /* Write single input byte as invalid sequence and skip it */
            if (!tlog_stream_write_seq(stream, false, buf, 1, pmeta, prem))
                break;
            buf++;
            len--;
        } else {
            /* If the (in)complete character doesn't fit into output */
            if (!tlog_stream_write_seq(stream, tlog_utf8_is_complete(utf8),
                                       utf8->buf, utf8->len,
                                       pmeta, prem)) {
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
tlog_stream_cut(struct tlog_stream *stream,
                uint8_t **pmeta, size_t *prem)
{
    struct tlog_utf8 *utf8;

    assert(tlog_stream_is_valid(stream));
    assert(pmeta != NULL);
    assert(*pmeta != NULL);
    assert(prem != NULL);
    assert(*prem <= (stream->size - stream->bin_len - stream->txt_len));

    utf8 = &stream->utf8;
    assert(!tlog_utf8_is_ended(utf8));

    if (tlog_stream_write_seq(stream, false, utf8->buf, utf8->len,
                              pmeta, prem)) {
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
