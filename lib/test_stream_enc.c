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
#include <tlog/test_misc.h>
#include <tlog/test_stream_enc.h>

#define BOOL_STR(_b) ((_b) ? "true" : "false")

bool
tlog_test_stream_enc(const char *n, const struct tlog_test_stream_enc t)
{
    bool passed = true;
    uint8_t obuf[TLOG_TEST_STREAM_ENC_BUF_SIZE] = {0,};
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

    if (memcmp(obuf, t.obuf_out, TLOG_TEST_STREAM_ENC_BUF_SIZE) != 0) {
        fprintf(stderr, "%s: obuf mismatch:\n", n);
        tlog_test_diff(stderr,
                       obuf, TLOG_TEST_STREAM_ENC_BUF_SIZE,
                       t.obuf_out, TLOG_TEST_STREAM_ENC_BUF_SIZE);
        passed = false;
    }

#undef CMP_SIZE
#undef CMP_BOOL
#undef FAIL
    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

    return passed;
}
