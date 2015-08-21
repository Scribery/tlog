/*
 * Tlog tlog_source test.
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
#include <unistd.h>
#include <string.h>
#include "tlog/rc.h"
#include "tlog/fd_reader.h"
#include "tlog/misc.h"
#include "tlog/source.h"
#include "test.h"

enum op_type {
    OP_TYPE_NONE,
    OP_TYPE_READ,
    OP_TYPE_LOC_GET,
    OP_TYPE_NUM
};

static const char*
op_type_to_str(enum op_type t)
{
    switch (t) {
        case OP_TYPE_NONE:
            return "none";
        case OP_TYPE_READ:
            return "read";
        case OP_TYPE_LOC_GET:
            return "loc_get";
        default:
            return "<unknown>";
    }
}

struct op_data_loc_get {
    size_t exp_loc;
};

struct op_data_read {
    int             exp_grc;
    struct tlog_pkt exp_pkt;
};

struct op {
    enum op_type type;
    union {
        struct op_data_loc_get  loc_get;
        struct op_data_read     read;
    } data;
};

struct test {
    const char     *input;
    const char     *hostname;
    const char     *username;
    unsigned int    session_id;
    size_t          io_size;
    struct op       op_list[16];
};

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
            fprintf(stream, "buf mismatch:");
            tlog_test_diff(stream,
                           res->data.io.buf, res->data.io.len,
                           exp->data.io.buf, exp->data.io.len);
        }
        break;
    default:
        break;
    }
}

static bool
test(const char *n, const struct test t)
{
    bool passed = true;
    int fd = -1;
    tlog_grc grc;
    struct tlog_reader *reader = NULL;
    struct tlog_source *source = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    char filename[] = "tlog_source_test.XXXXXX";
    const struct op *op;
    size_t input_len = strlen(t.input);
    size_t loc;

    fd = mkstemp(filename);
    if (fd < 0) {
        fprintf(stderr, "Failed opening a temporary file: %s\n",
                strerror(errno));
        exit(1);
    }
    if (unlink(filename) < 0) {
        fprintf(stderr, "Failed unlinking the temporary file: %s\n",
                strerror(errno));
        exit(1);
    }
    if (write(fd, t.input, input_len) != (ssize_t)input_len) {
        fprintf(stderr, "Failed writing the temporary file: %s\n",
                strerror(errno));
        exit(1);
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "Failed rewinding the temporary file: %s\n",
                strerror(errno));
        exit(1);
    }
    grc = tlog_reader_create(&reader, &tlog_fd_reader_type, fd);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating FD reader: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }
    grc = tlog_source_create(&source, reader,
                             t.hostname, t.username, t.session_id,
                             t.io_size);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating source: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }

#define FAIL(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, "%s: " _fmt "\n", n, ##_args);  \
        passed = false;                                 \
    } while (0)

#define FAIL_OP(_fmt, _args...) \
    FAIL("op #%zd (%s): " _fmt,                                 \
         op - t.op_list + 1, op_type_to_str(op->type), ##_args)

    for (op = t.op_list; op->type != OP_TYPE_NONE; op++) {
        switch (op->type) {
            case OP_TYPE_READ:
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
            case OP_TYPE_LOC_GET:
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
                }
                break;
            default:
                fprintf(stderr, "Unknown operation type: %d\n", op->type);
                exit(1);
        }
    }

#undef FAIL_OP
#undef FAIL

    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

    tlog_source_destroy(source);
    tlog_reader_destroy(reader);
    if (fd >= 0)
        close(fd);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define MSG_WINDOW_SPEC(_host_token, _user_token, _session_token, \
                        _id_token, _pos_token, _width_token, _height_token) \
    "{"                                                                     \
        "\"type\":"     "\"window\","                                       \
        "\"host\":"     "\"" #_host_token "\","                             \
        "\"user\":"     "\"" #_user_token "\","                             \
        "\"session\":"  #_session_token ","                                 \
        "\"id\":"       #_id_token ","                                      \
        "\"pos\":"      "\"" #_pos_token "\","                              \
        "\"width\":"    #_width_token ","                                   \
        "\"height\":"   #_height_token                                      \
    "}\n"

#define MSG_WINDOW_DUMMY(_id_token, _pos_token, \
                         _width_token, _height_token)       \
    MSG_WINDOW_SPEC(host, user, 1, _id_token, _pos_token,   \
                    _width_token, _height_token)

#define OP_NONE {.type = OP_TYPE_NONE}

#define OP_LOC_GET(_exp_loc) \
    {.type = OP_TYPE_LOC_GET,                       \
     .data = {.loc_get = {.exp_loc = _exp_loc}}}

#define PKT_VOID TLOG_PKT_VOID

#define PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height) \
    (struct tlog_pkt){                          \
        .timestamp  = {_tv_sec, _tv_nsec},      \
        .type       = TLOG_PKT_TYPE_WINDOW,     \
        .data       = {                         \
            .window = {                         \
                .width  = _width,               \
                .height = _height               \
            }                                   \
        }                                       \
    }

#define PKT_IO(_width, _height)

#define OP_READ(_exp_grc, _exp_pkt) \
    {.type = OP_TYPE_READ,                                          \
     .data = {.read = {.exp_grc = _exp_grc, .exp_pkt = _exp_pkt}}}

#define TEST_SPEC(_name_token, _input, _hostname, _username, _session_id, \
                  _io_size, _op_list_init_args...)                          \
    passed = test(#_name_token,                                             \
                  (struct test){                                            \
                    .input      = _input,                                   \
                    .hostname   = _hostname,                                \
                    .username   = _username,                                \
                    .session_id = _session_id,                              \
                    .io_size    = _io_size,                                 \
                    .op_list    = {_op_list_init_args, OP_NONE}             \
                  }                                                         \
                 ) && passed

#define TEST_ANY(_name_token, _input, _io_size, _op_list_init_args...) \
    TEST_SPEC(_name_token, _input, NULL, NULL, 0, _io_size,             \
              ##_op_list_init_args)

    TEST_ANY(null,
         "", 4,
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, PKT_VOID),
         OP_LOC_GET(1));

    TEST_ANY(window,
         MSG_WINDOW_DUMMY(1, 0.000, 100, 200),
         4,
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, PKT_WINDOW(0, 0, 100, 200)),
         OP_LOC_GET(2));

    return !passed;
}
