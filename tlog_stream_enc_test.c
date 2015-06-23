/*
 * Tlog tlog_stream_enc_* function test module.
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

#include <stdio.h>
#include <string.h>
#include "tlog_stream_enc_test.h"

#define BOOL_STR(_b) ((_b) ? "true" : "false")

static void
diff_side(FILE *stream, const char *name,
          const uint8_t *out, const uint8_t *exp, size_t len)
{
    size_t col;
    size_t i;
    const uint8_t *o;
    const uint8_t *e;
    uint8_t c;

    fprintf(stream, "%s str:\n", name);
    for (o = out, i = len; i > 0; o++, i--) {
        c = *o;
        fputc(((c >= 0x20 && c < 0x7f) ? c : ' '), stream);
    }
    fputc('\n', stream);
    for (o = out, e = exp, i = len; i > 0; o++, e++, i--)
        fprintf(stream, "%c", ((*o == *e) ? ' ' : '^'));

    fprintf(stream, "\n%s hex:\n", name);
    for (o = out, e = exp, i = len, col = 0; i > 0; o++, e++, i--) {
        fprintf(stream, " %c%02x", ((*o == *e) ? ' ' : '!'), *o);
        col++;
        if (col > 0xf) {
            col = 0;
            fprintf(stream, "\n");
        }
    }
    if (col != 0)
        fprintf(stream, "\n");
}

static void
diff(FILE *stream, const uint8_t *res, const uint8_t *exp, size_t len)
{
    diff_side(stream, "expected", exp, res, len);
    fprintf(stream, "\n");
    diff_side(stream, "result", res, exp, len);
}

bool
tlog_stream_enc_test(const char *n, const struct tlog_stream_enc_test t)
{
    bool passed = true;
    uint8_t obuf[TLOG_STREAM_ENC_TEST_BUF_SIZE] = {0,};
    size_t orem = t.orem_in;
    size_t olen = t.olen_in;
    size_t irun = t.irun_in;
    size_t idig = t.idig_in;
    bool fit;

#define FAIL(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, "%s: " _fmt "\n", n, ##_args);  \
        passed = false;                                 \
    } while (0)

#define CMP_SIZE(_name) \
    do {                                                        \
        if (_name != t._name##_out)                            \
            FAIL(#_name " %zu != %zu", _name, t._name##_out);  \
    } while (0)

#define CMP_BOOL(_name) \
    do {                                                        \
        if (_name != t._name##_out)                            \
            FAIL(#_name " %s != %s",                            \
                 BOOL_STR(_name), BOOL_STR(t._name##_out));    \
    } while (0)

    fit = t.func(obuf, &orem, &olen, &irun, &idig, t.ibuf_in, t.ilen_in);
    CMP_BOOL(fit);
    CMP_SIZE(orem);
    CMP_SIZE(olen);
    CMP_SIZE(irun);
    CMP_SIZE(idig);

    if (memcmp(obuf, t.obuf_out, TLOG_STREAM_ENC_TEST_BUF_SIZE) != 0) {
        fprintf(stderr, "%s: obuf mismatch:\n", n);
        diff(stderr, obuf, t.obuf_out, TLOG_STREAM_ENC_TEST_BUF_SIZE);
        passed = false;
    }

#undef CMP_SIZE
#undef CMP_BOOL
#undef FAIL
    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

    return passed;
}
