/*
 * Tlog tlog_json_stream_enc_bin function test.
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
#include <tltest/json_stream_enc.h>

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = tlog_test_json_stream_enc(__FILE__, __LINE__, #_name_token,    \
                                  (struct tlog_test_json_stream_enc)        \
                                       {.func = tlog_json_stream_enc_bin,   \
                                        ##_struct_init_args}) &&            \
             passed

    /* No input producing no output */
    TEST(zero,          .fit_out    = true);

    /* Output for one byte input */
    TEST(one,           .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 5,
                        .olen_out   = 3,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* One byte input, output short of one byte */
    TEST(one_out_one,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "",
                        .orem_in    = 4,
                        .orem_out   = 4);

    /* One byte input, output short of two bytes */
    TEST(one_out_two,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "",
                        .orem_in    = 3,
                        .orem_out   = 3);

    /* One byte input, output short of three (all) bytes */
    TEST(one_out_three, .ibuf_in    = {0xff},
                        .ilen_in    = 1);

    /* Output for two bytes input */
    TEST(two,           .ibuf_in    = {0x01, 0x02},
                        .ilen_in    = 2,
                        .obuf_out   = "1,2",
                        .orem_in    = 5,
                        .olen_out   = 3,
                        .irun_out   = 2,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Two byte input, output short of one byte */
    TEST(two_out_one,   .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,",
                        .orem_in    = 8,
                        .orem_out   = 8);

    /* Two byte input, output short of two bytes */
    TEST(two_out_two,   .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,",
                        .orem_in    = 7,
                        .orem_out   = 7);

    /*
     * Two byte input, output short of three bytes
     * (no space for second input byte in the output at all)
     */
    TEST(two_out_three, .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254,",
                        .orem_in    = 6,
                        .orem_out   = 6);

    /*
     * Two byte input, output short of four bytes (no space for comma and
     * second input byte in the output)
     */
    TEST(two_out_four,  .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "254",
                        .orem_in    = 5,
                        .orem_out   = 5);

    /*
     * Two byte input, output short of five bytes
     * (no space even for the first complete byte)
     */
    TEST(two_out_five,  .ibuf_in    = {0xfe, 0xff},
                        .ilen_in    = 2,
                        .obuf_out   = "",
                        .orem_in    = 4,
                        .orem_out   = 4);

    /* Output for three bytes input */
    TEST(three,         .ibuf_in    = {0x01, 0x02, 0x03},
                        .ilen_in    = 3,
                        .obuf_out   = "1,2,3",
                        .orem_in    = 7,
                        .olen_out   = 5,
                        .irun_out   = 3,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Non-zero input olen */
    TEST(non_zero_olen, .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = ",255",
                        .orem_in    = 6,
                        .olen_in    = 10,
                        .olen_out   = 14,
                        .irun_out   = 1,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Non-zero input irun */
    TEST(non_zero_irun, .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 3,
                        .olen_out   = 3,
                        .irun_in    = 1,
                        .irun_out   = 2,
                        .idig_in    = 10,
                        .idig_out   = 10,
                        .fit_out    = true);

    /* Rolling over to second digit */
    TEST(second_digit,  .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 4,
                        .olen_out   = 3,
                        .irun_in    = 9,
                        .irun_out   = 10,
                        .idig_in    = 10,
                        .idig_out   = 100,
                        .fit_out    = true);

    /* Not rolling over to second digit */
    TEST(second_digit_abort,    .ibuf_in    = {0xff},
                                .ilen_in    = 1,
                                .obuf_out   = "",
                                .orem_in    = 3,
                                .orem_out   = 3,
                                .irun_in    = 9,
                                .irun_out   = 9,
                                .idig_in    = 10,
                                .idig_out   = 10,
                                .fit_out    = false);

    /* Rolling over to third digit */
    TEST(third_digit,   .ibuf_in    = {0xff},
                        .ilen_in    = 1,
                        .obuf_out   = "255",
                        .orem_in    = 4,
                        .olen_out   = 3,
                        .irun_in    = 99,
                        .irun_out   = 100,
                        .idig_in    = 100,
                        .idig_out   = 1000,
                        .fit_out    = true);

    return !passed;
}
