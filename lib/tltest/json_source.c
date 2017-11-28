/*
 * Tlog_json_source test functions.
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

#include <tltest/json_source.h>
#include <tlog/mem_json_reader.h>
#include <tlog/misc.h>
#include <tlog/json_source.h>
#include <tltest/misc.h>
#include <tlog/rc.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

const char*
tltest_json_source_op_type_to_str(enum tltest_json_source_op_type type)
{
    switch (type) {
    case TLTEST_JSON_SOURCE_OP_TYPE_NONE:
        return "none";
    case TLTEST_JSON_SOURCE_OP_TYPE_READ:
        return "read";
    case TLTEST_JSON_SOURCE_OP_TYPE_LOC_GET:
        return "loc_get";
    default:
        return "<unknown>";
    }
}

static void
pkt_diff(FILE *stream,
         const struct tlog_pkt *res,
         const struct tlog_pkt *exp)
{
    assert(tlog_pkt_is_valid(res));
    assert(tlog_pkt_is_valid(exp));

    if (res->timestamp.tv_sec != exp->timestamp.tv_sec ||
        res->timestamp.tv_nsec != exp->timestamp.tv_nsec) {
        fprintf(stream, "timestamp: %lld.%09ld != %lld.%09ld\n",
                (long long int)res->timestamp.tv_sec,
                (long int)res->timestamp.tv_nsec,
                (long long int)exp->timestamp.tv_sec,
                (long int)exp->timestamp.tv_nsec);
    }

    fprintf(stream, "type: %s %c= %s\n",
            tlog_pkt_type_to_str(res->type),
            (res->type == exp->type ? '=' : '!'),
            tlog_pkt_type_to_str(exp->type));
    if (res->type != exp->type) {
        return;
    }

    switch (res->type) {
    case TLOG_PKT_TYPE_WINDOW:
        if (res->data.window.width != exp->data.window.width) {
            fprintf(stream, "width: %hu != %hu\n",
                    res->data.window.width, exp->data.window.width);
        }
        if (res->data.window.height != exp->data.window.height) {
            fprintf(stream, "height: %hu != %hu\n",
                    res->data.window.height, exp->data.window.height);
        }
        break;
    case TLOG_PKT_TYPE_IO:
        if (res->data.io.output != exp->data.io.output) {
            fprintf(stream, "output: %s != %s\n",
                    (res->data.io.output ? "true" : "false"),
                    (exp->data.io.output ? "true" : "false"));
        }
        if (res->data.io.len != exp->data.io.len) {
            fprintf(stream, "len: %zu != %zu\n",
                    res->data.io.len, exp->data.io.len);
        }
        if (res->data.io.len != exp->data.io.len ||
            memcmp(res->data.io.buf, exp->data.io.buf,
                   res->data.io.len) != 0) {
            fprintf(stream, "buf mismatch:\n");
            tltest_diff(stream,
                        res->data.io.buf, res->data.io.len,
                        exp->data.io.buf, exp->data.io.len);
        }
        break;
    default:
        break;
    }
}

bool
tltest_json_source_run(
        const char                             *name,
        const char                             *input_buf,
        size_t                                  input_len,
        const struct tltest_json_source_output *output)
{
    bool passed = true;
    tlog_grc grc;
    struct tlog_json_reader *reader = NULL;
    struct tlog_source *source = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    const struct tltest_json_source_op *op;
    size_t loc;

    grc = tlog_mem_json_reader_create(&reader, input_buf, input_len);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating JSON memory reader: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }
    {
        struct tlog_json_source_params params = {
            .reader         = reader,
            .reader_owned   = false,
            .hostname       = output->hostname,
            .username       = output->username,
            .terminal       = output->terminal,
            .session_id     = output->session_id,
            .io_size        = output->io_size,
        };
        grc = tlog_json_source_create(&source, &params);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed creating source: %s\n",
                    tlog_grc_strerror(grc));
            exit(1);
        }
    }

#define FAIL(_fmt, _args...) \
    do {                                                    \
        fprintf(stderr, "%s: " _fmt "\n", name, ##_args);   \
        passed = false;                                     \
    } while (0)

#define FAIL_OP(_fmt, _args...) \
    FAIL("op #%zd (%s): " _fmt,                                 \
         op - output->op_list + 1,                              \
         tltest_json_source_op_type_to_str(op->type), ##_args)

    for (op = output->op_list;
         op->type != TLTEST_JSON_SOURCE_OP_TYPE_NONE;
         op++) {
        switch (op->type) {
        case TLTEST_JSON_SOURCE_OP_TYPE_READ:
            grc = tlog_source_read(source, &pkt);
            if (grc != op->data.read.exp_grc) {
                const char *res_str;
                const char *exp_str;
                res_str = tlog_grc_strerror(grc);
                exp_str = tlog_grc_strerror(op->data.read.exp_grc);
                FAIL_OP("grc: %s (%d) != %s (%d)",
                        res_str, grc,
                        exp_str, op->data.read.exp_grc);
            }
            if (!tlog_pkt_is_equal(&pkt, &op->data.read.exp_pkt)) {
                FAIL_OP("packet mismatch:");
                pkt_diff(stderr, &pkt, &op->data.read.exp_pkt);
            }
            tlog_pkt_cleanup(&pkt);
            break;
        case TLTEST_JSON_SOURCE_OP_TYPE_LOC_GET:
            loc = tlog_source_loc_get(source);
            if (loc != op->data.loc_get.exp_loc) {
                char *res_str;
                char *exp_str;
                res_str = tlog_source_loc_fmt(source, loc);
                exp_str = tlog_source_loc_fmt(source,
                                              op->data.loc_get.exp_loc);
                FAIL_OP("loc: %s (%zu) != %s (%zu)",
                        res_str, loc,
                        exp_str, op->data.loc_get.exp_loc);
                free(res_str);
                free(exp_str);
            }
            break;
        default:
            fprintf(stderr, "Unknown operation type: %d\n", op->type);
            exit(1);
        }
    }

#undef FAIL_OP
#undef FAIL

    tlog_source_destroy(source);
    tlog_json_reader_destroy(reader);
    return passed;
}

bool
tltest_json_source(const char *file, int line, const char *name,
                   const struct tltest_json_source test)
{
    bool passed;

    passed = tltest_json_source_run(name,
                                       test.input, strlen(test.input),
                                       &test.output);
    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, name);
    return passed;
}
