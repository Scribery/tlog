/*
 * Tlog tlog_sink test functions.
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

#include <tlog/test_sink.h>
#include <tlog/mem_writer.h>
#include <tlog/sink.h>
#include <tlog/misc.h>
#include <tlog/test_misc.h>

#define SIZE    TLOG_SINK_CHUNK_SIZE_MIN

const char*
tlog_test_sink_op_type_to_str(enum tlog_test_sink_op_type type)
{
    switch (type) {
    case TLOG_TEST_SINK_OP_TYPE_NONE:
        return "none";
    case TLOG_TEST_SINK_OP_TYPE_WRITE:
        return "write";
    case TLOG_TEST_SINK_OP_TYPE_FLUSH:
        return "flush";
    case TLOG_TEST_SINK_OP_TYPE_CUT:
        return "cut";
    default:
        return "<unknown>";
    }
}

bool
tlog_test_sink(const char *name, const struct tlog_test_sink test)
{
    bool passed = true;
    tlog_grc grc;
    struct tlog_writer *writer = NULL;
    struct tlog_sink sink;
    const struct tlog_test_sink_op *op;
    size_t exp_output_len;
    size_t res_output_len;
    char *res_output = NULL;

    grc = tlog_mem_writer_create(&writer, &res_output, &res_output_len);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating memory writer: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }

    grc = tlog_sink_init(&sink, writer, test.hostname, test.username,
                         test.session_id, SIZE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed initializing the sink: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    };

#define FAIL(_fmt, _args...) \
    do {                                                    \
        fprintf(stderr, "%s: " _fmt "\n", name, ##_args);   \
        passed = false;                                     \
    } while (0)

#define FAIL_OP \
    do {                                                \
        FAIL("op #%zd (%s) failed",                     \
             op - test.op_list + 1,                     \
             tlog_test_sink_op_type_to_str(op->type));  \
        goto cleanup;                                   \
    } while (0)

#define CHECK_OP(_expr) \
    do {                            \
        if ((_expr) != TLOG_RC_OK)  \
            FAIL_OP;                \
    } while (0)

    for (op = test.op_list; op->type != TLOG_TEST_SINK_OP_TYPE_NONE; op++) {
        switch (op->type) {
        case TLOG_TEST_SINK_OP_TYPE_WRITE:
            CHECK_OP(tlog_sink_write(&sink, &op->data.write));
            break;
        case TLOG_TEST_SINK_OP_TYPE_FLUSH:
            CHECK_OP(tlog_sink_flush(&sink));
            break;
        case TLOG_TEST_SINK_OP_TYPE_CUT:
            CHECK_OP(tlog_sink_cut(&sink));
            break;
        default:
            fprintf(stderr, "Unknown operation type: %d\n", op->type);
            exit(1);
        }
    }

#undef CHECK_OP
#undef FAIL_OP

    exp_output_len = strlen((const char *)test.output);

    if (res_output_len != exp_output_len ||
        memcmp(res_output, test.output, res_output_len) != 0) {
        fprintf(stderr, "%s: output mismatch:\n", name);
        tlog_test_diff(stderr,
                       (const uint8_t *)res_output, res_output_len,
                       (const uint8_t *)test.output, exp_output_len);
        passed = false;
    }
#undef FAIL

    fprintf(stderr, "%s: %s\n", name, (passed ? "PASS" : "FAIL"));

cleanup:
    tlog_writer_destroy(writer);
    free(res_output);
    tlog_sink_cleanup(&sink);
    return passed;
}

