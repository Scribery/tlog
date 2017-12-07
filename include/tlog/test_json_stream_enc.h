/**
 * @file
 * @brief tlog_json_stream_enc_* function test module.
 *
 * A module for testing tlog_json_stream_enc_txt/bin functions.
 */
/*
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

#ifndef _TLOG_TEST_JSON_STREAM_ENC_H
#define _TLOG_TEST_JSON_STREAM_ENC_H

#include <stdbool.h>
#include <stdint.h>
#include <tlog/json_stream.h>

#define TLOG_TEST_JSON_STREAM_ENC_BUF_SIZE    32

typedef bool (*tlog_test_json_stream_enc_func)(
                                tlog_trx_state trx,
                                struct tlog_json_dispatcher *dispatcher,
                                uint8_t *obuf, size_t *polen,
                                size_t *pirun, size_t *pidig,
                                const uint8_t *ibuf, size_t ilen);

struct tlog_test_json_stream_enc {
    tlog_test_json_stream_enc_func  func;
    const uint8_t   ibuf_in[TLOG_TEST_JSON_STREAM_ENC_BUF_SIZE];
    size_t          ilen_in;
    const uint8_t   obuf_out[TLOG_TEST_JSON_STREAM_ENC_BUF_SIZE];
    size_t          orem_in;
    size_t          orem_out;
    size_t          olen_in;
    size_t          olen_out;
    size_t          irun_in;
    size_t          irun_out;
    size_t          idig_in;
    size_t          idig_out;
    bool            fit_out;    /**< Expected func return value */
};

/**
 * Run a tlog_json_stream_enc_* function test.
 *
 * @param n     Test name.
 * @param t     Test structure.
 *
 * @return True if test passed, false otherwise.
 */
extern bool tlog_test_json_stream_enc(
                        const char *file,
                        int line,
                        const char *n,
                        const struct tlog_test_json_stream_enc t);

#endif /* _TLOG_TEST_JSON_STREAM_ENC_H */
