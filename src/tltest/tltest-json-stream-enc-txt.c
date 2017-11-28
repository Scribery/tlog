/*
 * Tlog tlog_json_stream_enc_txt function test.
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
#include <tltest/json_stream_enc.h>

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = tltest_json_stream_enc(__FILE__, __LINE__, #_name_token,       \
                                    (struct tltest_json_stream_enc)         \
                                       {.func = tlog_json_stream_enc_txt,   \
                                        ##_struct_init_args}) &&            \
             passed

    /* No input producing no output */
    TEST(zero,          .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Output for one byte input */
    TEST(one,           .ibuf_in    = {'A'},
                        .ilen_in    = 1,
                        .obuf_out   = "A",
                        .orem_in    = 3,
                        .olen_out   = 1,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* One byte input, output zero */
    TEST(one_out_zero,  .ibuf_in    = {'A'},
                        .ilen_in    = 1,
                        .idig_in    = 10,
                        .idig_out   = 10);

    /* Output for two byte input */
    TEST(two,           .ibuf_in    = {0xd0, 0x90},
                        .ilen_in    = 2,
                        .obuf_out   = "А",
                        .orem_in    = 4,
                        .olen_out   = 2,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Two byte input, one byte output */
    TEST(two_out_one,   .ibuf_in    = {0xd0, 0x90},
                        .ilen_in    = 2,
                        .orem_in    = 1,
                        .orem_out   = 1);

    /* Two byte input, zero byte output */
    TEST(two_out_zero,  .ibuf_in    = {0xd0, 0x90},
                        .ilen_in    = 2);

    /* Output for three byte input */
    TEST(three,         .ibuf_in    = {0xe5, 0x96, 0x9c},
                        .ilen_in    = 3,
                        .obuf_out   = "喜", /* Chinese "like/love/..." */
                        .orem_in    = 5,
                        .olen_out   = 3,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Output for four byte input */
    TEST(four,          .ibuf_in    = {0xf0, 0x9d, 0x84, 0x9e},
                        .ilen_in    = 4,
                        .obuf_out   = "𝄞", /* G Clef */
                        .orem_in    = 6,
                        .olen_out   = 4,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Rolling over to second digit */
    TEST(second_digit,  .ibuf_in    = {'A'},
                        .ilen_in    = 1,
                        .obuf_out   = "A",
                        .orem_in    = 2,
                        .olen_out   = 1,
                        .irun_in    = 9,
                        .irun_out   = 10,
                        .idig_in    = 10,
                        .idig_out   = 100,
                        .fit_out    = true);

    /* Not rolling over to second digit */
    TEST(second_digit_abort,    .ibuf_in    = {'A'},
                                .ilen_in    = 1,
                                .orem_in    = 1,
                                .orem_out   = 1,
                                .irun_in    = 9,
                                .irun_out   = 9,
                                .idig_in    = 10,
                                .idig_out   = 10,
                                .fit_out    = false);

    /* Rolling over to third digit */
    TEST(third_digit,   .ibuf_in    = {'A'},
                        .ilen_in    = 1,
                        .obuf_out   = "A",
                        .orem_in    = 2,
                        .olen_out   = 1,
                        .irun_in    = 99,
                        .irun_out   = 100,
                        .idig_in    = 100,
                        .idig_out   = 1000,
                        .fit_out    = true);

#define TEST_ESC(_char_code_token, _obuf_out) \
    TEST(esc_ ##_char_code_token,   .ibuf_in = {_char_code_token},      \
                                    .ilen_in = 1,                       \
                                    .obuf_out = _obuf_out,              \
                                    .orem_in = strlen(_obuf_out) + 2,   \
                                    .olen_out = strlen(_obuf_out),      \
                                    .irun_out = 1,                      \
                                    .idig_in = 10,                      \
                                    .idig_out = 10,                     \
                                    .fit_out = true)

    /* Control character escaping */
    TEST_ESC(0x00, "\\u0000");
    TEST_ESC(0x01, "\\u0001");
    TEST_ESC(0x02, "\\u0002");
    TEST_ESC(0x03, "\\u0003");
    TEST_ESC(0x04, "\\u0004");
    TEST_ESC(0x05, "\\u0005");
    TEST_ESC(0x06, "\\u0006");
    TEST_ESC(0x07, "\\u0007");
    TEST_ESC(0x08, "\\b");
    TEST_ESC(0x09, "\\t");
    TEST_ESC(0x0a, "\\n");
    TEST_ESC(0x0b, "\\u000b");
    TEST_ESC(0x0c, "\\f");
    TEST_ESC(0x0d, "\\r");
    TEST_ESC(0x0e, "\\u000e");
    TEST_ESC(0x0f, "\\u000f");
    TEST_ESC(0x10, "\\u0010");
    TEST_ESC(0x11, "\\u0011");
    TEST_ESC(0x12, "\\u0012");
    TEST_ESC(0x13, "\\u0013");
    TEST_ESC(0x14, "\\u0014");
    TEST_ESC(0x15, "\\u0015");
    TEST_ESC(0x16, "\\u0016");
    TEST_ESC(0x17, "\\u0017");
    TEST_ESC(0x18, "\\u0018");
    TEST_ESC(0x19, "\\u0019");
    TEST_ESC(0x1a, "\\u001a");
    TEST_ESC(0x1b, "\\u001b");
    TEST_ESC(0x1c, "\\u001c");
    TEST_ESC(0x1d, "\\u001d");
    TEST_ESC(0x1e, "\\u001e");
    TEST_ESC(0x1f, "\\u001f");
    TEST_ESC(0x22, "\\\"");
    TEST_ESC(0x5c, "\\\\");
    TEST_ESC(0x7f, "\\u007f");

    return !passed;
}
