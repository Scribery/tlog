/*
 * Tlog pass-through test.
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

#include "config.h"
#include <stdio.h>
#include <tlog/test_sink.h>
#include <tlog/test_source.h>

struct tlog_test_passthrough {
    struct tlog_test_sink_input     input;
    struct tlog_test_source_output  output;
};

static bool
tlog_test_passthrough(const char *name,
                      struct tlog_test_passthrough test)
{
    bool passed = true;
    char *sink_name = NULL;
    char *source_name = NULL;
    char *log_buf = NULL;
    size_t log_len = 0;

    asprintf(&sink_name, "%s: sink", name);
    asprintf(&source_name, "%s: source", name);

    passed = tlog_test_sink_run(sink_name,
                                &test.input, &log_buf, &log_len) &&
             passed;

    passed = tlog_test_source_run(source_name,
                                  log_buf, log_len, &test.output) &&
             passed;

    free(sink_name);
    free(source_name);
    free(log_buf);

    fprintf(stderr, "%s: %s\n", name, (passed ? "PASS" : "FAIL"));
    return passed;
}

int
main(void)
{
    bool passed = true;

#define PKT_VOID \
    TLOG_PKT_VOID
#define PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height) \
    TLOG_PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height)
#define PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len) \
    TLOG_PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len)
#define PKT_IO_STR(_tv_sec, _tv_nsec, _output, _buf) \
    TLOG_PKT_IO_STR(_tv_sec, _tv_nsec, _output, _buf)

#define OP_WRITE(_pkt)  TLOG_TEST_SINK_OP_WRITE(_pkt)
#define OP_FLUSH        TLOG_TEST_SINK_OP_FLUSH
#define OP_CUT          TLOG_TEST_SINK_OP_CUT

#define OP_LOC_GET(_exp_loc) \
    TLOG_TEST_SOURCE_OP_LOC_GET(_exp_loc)
#define OP_READ(_exp_grc, _exp_pkt) \
    TLOG_TEST_SOURCE_OP_READ(_exp_grc, _exp_pkt)

#define TEST(_name_token, _struct_init_args...) \
    passed = tlog_test_passthrough(                 \
                #_name_token,                       \
                (struct tlog_test_passthrough){     \
                    _struct_init_args,              \
                    .input.hostname = "localhost",  \
                    .input.username = "user",       \
                    .input.session_id = 1,          \
                    .output.hostname = NULL,        \
                    .output.username = NULL,        \
                    .output.session_id = 0          \
                }                                   \
             ) && passed

    TEST(null,
         .input.op_list = {},
         .output.io_size = 4,
         .output.op_list = {});

    return !passed;
}
