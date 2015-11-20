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
#include <tlog/rc.h>
#include <tlog/mem_reader.h>
#include <tlog/misc.h>
#include <tlog/source.h>
#include <tlog/test_misc.h>

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
    tlog_grc grc;
    struct tlog_reader *reader = NULL;
    struct tlog_source *source = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    const struct op *op;
    size_t loc;

    grc = tlog_mem_reader_create(&reader, t.input, strlen(t.input));
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating memory reader: %s\n",
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

    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

    tlog_source_destroy(source);
    tlog_reader_destroy(reader);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define MSG_SPEC(_host_token, _user_token, _session_token, \
                 _id_token, _pos_token,                         \
                 _timing, _in_txt, _in_bin, _out_txt, _out_bin) \
    "{"                                                         \
        "\"host\":"     "\"" #_host_token "\","                 \
        "\"user\":"     "\"" #_user_token "\","                 \
        "\"session\":"  #_session_token ","                     \
        "\"id\":"       #_id_token ","                          \
        "\"pos\":"      #_pos_token ","                         \
        "\"timing\":"   "\"" _timing "\","                      \
        "\"in_txt\":"   "\"" _in_txt "\","                      \
        "\"in_bin\":"   "[" _in_bin "],"                        \
        "\"out_txt\":"  "\"" _out_txt "\","                     \
        "\"out_bin\":"  "[" _out_bin "]"                        \
    "}\n"

#define MSG_DUMMY(_id_token, _pos_token, \
                  _timing, _in_txt, _in_bin, _out_txt, _out_bin)    \
    MSG_SPEC(host, user, 1, _id_token, _pos_token,                  \
                 _timing, _in_txt, _in_bin, _out_txt, _out_bin)


#define OP_NONE {.type = OP_TYPE_NONE}

#define OP_LOC_GET(_exp_loc) \
    {.type = OP_TYPE_LOC_GET,                       \
     .data = {.loc_get = {.exp_loc = _exp_loc}}}

#define PKT_VOID TLOG_PKT_VOID

#define PKT_WINDOW(_tv_sec, _tv_nsec, _width, _height) \
    (struct tlog_pkt){                                  \
        .timestamp  = {_tv_sec, _tv_nsec},              \
        .type       = TLOG_PKT_TYPE_WINDOW,             \
        .data       = {                                 \
            .window = {                                 \
                .width  = _width,                       \
                .height = _height                       \
            }                                           \
        }                                               \
    }

#define PKT_IO(_tv_sec, _tv_nsec, _output, _buf, _len) \
    (struct tlog_pkt){                                  \
        .timestamp  = {_tv_sec, _tv_nsec},              \
        .type       = TLOG_PKT_TYPE_IO,                 \
        .data       = {                                 \
            .io = {                                     \
                .output     = _output,                  \
                .buf        = (uint8_t *)_buf,          \
                .buf_owned  = false,                    \
                .len        = _len                      \
            }                                           \
        }                                               \
    }


#define PKT_IO_STR(_tv_sec, _tv_nsec, _output, _buf) \
    (struct tlog_pkt){                                  \
        .timestamp  = {_tv_sec, _tv_nsec},              \
        .type       = TLOG_PKT_TYPE_IO,                 \
        .data       = {                                 \
            .io = {                                     \
                .output     = _output,                  \
                .buf        = (uint8_t *)_buf,          \
                .buf_owned  = false,                    \
                .len        = strlen(_buf)              \
            }                                           \
        }                                               \
    }


#define OP_READ(_exp_grc, _exp_pkt) \
    {.type = OP_TYPE_READ,                                          \
     .data = {.read = {.exp_grc = _exp_grc, .exp_pkt = _exp_pkt}}}

#define OP_READ_OK(_args...) OP_READ(TLOG_RC_OK, ##_args)

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

    TEST_ANY(null, "", 4, OP_NONE);

    TEST_ANY(null_eof,
         "", 4,
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(null_repeat_eof,
         "", 4,
         OP_READ_OK(PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(window,
         MSG_DUMMY(1, 1000, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200))
    );

    TEST_ANY(window_eof,
         MSG_DUMMY(1, 1000, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(two_windows,
         MSG_DUMMY(1, 1000, "=110x120=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ_OK(PKT_WINDOW(1, 0, 210, 220))
    );

    TEST_ANY(two_windows_eof,
         MSG_DUMMY(1, 1000, "=100x200=300x400", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_WINDOW(1, 0, 300, 400)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(loc_null,
         "",
         4,
         OP_LOC_GET(1)
    );

    TEST_ANY(loc_null_after_eof,
         "",
         4,
         OP_READ_OK(PKT_VOID),
         OP_LOC_GET(1)
    );

    TEST_ANY(loc_after_window,
         MSG_DUMMY(1, 1000, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_LOC_GET(2)
    );

    TEST_ANY(loc_after_two_windows,
         MSG_DUMMY(1, 1000, "=100x200=300x400", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_WINDOW(1, 0, 300, 400)),
         OP_LOC_GET(2)
    );

    TEST_ANY(pos_0,
         MSG_DUMMY(1, 0, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(0, 0, 100, 200))
    );

    TEST_ANY(pos_1ms,
         MSG_DUMMY(1, 1, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(0, 1000000, 100, 200))
    );

    TEST_ANY(pos_999ms,
         MSG_DUMMY(1, 999, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(0, 999000000, 100, 200))
    );

    TEST_ANY(pos_1s,
         MSG_DUMMY(1, 1000, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200))
    );

    TEST_ANY(pos_max,
         MSG_DUMMY(1, 9223372036854775807, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(9223372036854775, 807000000, 100, 200))
    );

    TEST_ANY(pos_overflow,
         MSG_DUMMY(1, 9223372036854775808, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(9223372036854775, 807000000, 100, 200))
    );

    TEST_ANY(pos_negative_1ms,
         MSG_DUMMY(1, -1, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(0, -1000000, 100, 200))
    );

    TEST_ANY(pos_negative_999ms,
         MSG_DUMMY(1, -999, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(0, -999000000, 100, 200))
    );

    TEST_ANY(pos_negative_1s,
         MSG_DUMMY(1, -1000, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(-1, 0, 100, 200))
    );

    TEST_ANY(pos_min,
         MSG_DUMMY(1, -9223372036854775808, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(-9223372036854775, -808000000, 100, 200))
    );

    TEST_ANY(pos_underflow,
         MSG_DUMMY(1, -9223372036854775809, "=100x200", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_OK,
                 PKT_WINDOW(-9223372036854775, -808000000, 100, 200))
    );

    TEST_ANY(syntax_error,
         MSG_DUMMY(aaa, bbb, "", "", "", "", ""),
         4,
         OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                 PKT_VOID)
    );

    TEST_ANY(loc_after_syntax_error,
         MSG_DUMMY(aaa, bbb, "", "", "", "", ""),
         4,
         OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                 PKT_VOID),
         OP_LOC_GET(2)
    );

    TEST_ANY(syntax_error_recovery,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(aaa, bbb, "", "", "", "", "")
         MSG_DUMMY(2, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                 PKT_VOID),
         OP_READ_OK(PKT_WINDOW(2, 0, 210, 220))
    );

    TEST_ANY(schema_error_recovery,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         "{\"abc\": \"def\"}\n"
         MSG_DUMMY(2, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_MSG_FIELD_MISSING, PKT_VOID),
         OP_READ_OK(PKT_WINDOW(2, 0, 210, 220))
    );

    TEST_ANY(type_error_recovery,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY("0", 0, "", "", "", "", "")
         MSG_DUMMY(2, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_TYPE, PKT_VOID),
         OP_READ_OK(PKT_WINDOW(2, 0, 210, 220))
    );

    TEST_ANY(value_error_recovery,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(2, 2000, "=65536x0", "", "", "", "")
         MSG_DUMMY(3, 3000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_WINDOW(3, 0, 210, 220))
    );

    TEST_ANY(window_repeat_eof,
         MSG_DUMMY(1, 1000, "=100x200", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_null,
         MSG_DUMMY(1, 1000, "", "", "", "", ""),
         4,
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt1,
         MSG_DUMMY(1, 1000, "<1", "X", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_out_1_txt1,
         MSG_DUMMY(1, 1000, ">1", "", "", "X", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, true, "X")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_mixed_txt,
         MSG_DUMMY(1, 1000, "<1>1<1>1", "AC", "", "BD", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "A")),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "B")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "C")),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "D")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_spaced_timing,
         MSG_DUMMY(1, 1, " <1 +1 >1 +1 <1 +1 >1 ", "AC", "", "BD", ""),
         4,
         OP_READ_OK(PKT_IO_STR(0, 1000000, false, "A")),
         OP_READ_OK(PKT_IO_STR(0, 2000000, true, "B")),
         OP_READ_OK(PKT_IO_STR(0, 3000000, false, "C")),
         OP_READ_OK(PKT_IO_STR(0, 4000000, true, "D")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_mixed_bin,
         MSG_DUMMY(1, 1000, "[0/1]0/1[0/1]0/1", "", "65,67", "", "66,68"),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "A")),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "B")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "C")),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "D")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_2_txt1,
         MSG_DUMMY(1, 1000, "<2", "XY", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "XY")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_out_2_txt1,
         MSG_DUMMY(1, 1000, ">2", "", "", "XY", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, true, "XY")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt2,
         MSG_DUMMY(1, 1000, "<1", "\xd0\x90", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xd0\x90")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt3,
         MSG_DUMMY(1, 1000, "<1", "\xe5\x96\x9c", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xe5\x96\x9c")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt4,
         MSG_DUMMY(1, 1000, "<1", "\xf0\x9d\x84\x9e", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x84\x9e")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_txt_split,
         MSG_DUMMY(1, 1000, "<3",
                      "\xf0\x9d\x85\x9d"
                      "\xf0\x9d\x85\x9e"
                      "\xf0\x9d\x85\x9f",
                      "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9d")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9e")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9f")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_txt_split_uneven,
         MSG_DUMMY(1, 1000, "<7",
                      "12345"
                      "\xf0\x9d\x85\x9d"
                      "\xf0\x9d\x85\x9e",
                      "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "1234")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "5")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9d")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9e")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_txt,
         MSG_DUMMY(1, 1000, "<1<2<3", "ABCXYZ", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "ABCXYZ")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_bin,
         MSG_DUMMY(1, 1000, "[0/1[0/2[0/3",
                      "", "49,50,51,52,53,54", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "123456")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_mixed,
         MSG_DUMMY(1, 1000, "<1[0/1<1[0/2<1",
                      "136", "50,52,53", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "123456")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_zero_delay,
         MSG_DUMMY(1, 1000, "<1+0<1", "AB", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "AB")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_zero_out,
         MSG_DUMMY(1, 1000, "<1>0<1", "AB", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "AB")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_break_out,
         MSG_DUMMY(1, 1000, "<1>1<1", "AC", "", "B", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "A")),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "B")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "C")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_merge_break_delay,
         MSG_DUMMY(1, 1000, "<1+1000<1", "AB", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "A")),
         OP_READ_OK(PKT_IO_STR(2, 0, false, "B")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_0_txt_1_bin,
         MSG_DUMMY(1, 1000, "[0/1<1", "Y", "88", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "XY")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_0_txt_2_bin,
         MSG_DUMMY(1, 1000, "[0/2<1", "Z", "88, 89", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "XYZ")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt1_1_bin,
         MSG_DUMMY(1, 1000, "[1/1", "X", "89", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "Y")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_1_txt1_1_bin,
         MSG_DUMMY(1, 1000, "[1/1", "X", "89", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "Y")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_2_txt1_1_bin,
         MSG_DUMMY(1, 1000, "[2/1", "XY", "90", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "Z")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_delay_on_boundary,
         MSG_DUMMY(1, 1000, "<4+1000<4", "12345678", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "1234")),
         OP_READ_OK(PKT_IO_STR(2, 0, false, "5678")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_delay_before_uneven_boundary,
         MSG_DUMMY(1, 1000, "<1+1000<2",
                      "X\xf0\x9d\x85\x9dY", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(2, 0, false, "\xf0\x9d\x85\x9d")),
         OP_READ_OK(PKT_IO_STR(2, 0, false, "Y")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_delay_after_uneven_boundary,
         MSG_DUMMY(1, 1000, "<2+1000<1",
                      "X\xf0\x9d\x85\x9dY", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "\xf0\x9d\x85\x9d")),
         OP_READ_OK(PKT_IO_STR(2, 0, false, "Y")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_short_char,
         MSG_DUMMY(1, 1000, "<1", "\xf0\x9d\x85", "", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_short_bin,
         MSG_DUMMY(1, 1000, "[0/2", "", "48", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_bin_not_int,
         MSG_DUMMY(1, 1000, "[0/1", "", "3.14", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_bin_void,
         MSG_DUMMY(1, 1000, "[0/0", "", "", "", ""),
         4,
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_txt_too_short,
         MSG_DUMMY(1, 1000, "<1", "", "", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID)
    );

    TEST_ANY(io_in_txt_too_long_0,
         MSG_DUMMY(1, 1000, "", "X", "", "", ""),
         4,
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_txt_too_long_1,
         MSG_DUMMY(1, 1000, "<1", "XY", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X"))
    );

    TEST_ANY(io_in_bin_min,
         MSG_DUMMY(1, 1000, "[0/1", "", "0", "", ""),
         4,
         OP_READ_OK(PKT_IO(1, 0, false, "\0", 1)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_bin_max,
         MSG_DUMMY(1, 1000, "[0/1", "", "255", "", ""),
         4,
         OP_READ_OK(PKT_IO(1, 0, false, "\xff", 1)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_bin_too_small,
         MSG_DUMMY(1, 1000, "[0/1", "", "-1", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_bin_too_big,
         MSG_DUMMY(1, 1000, "[0/1", "", "256", "", ""),
         4,
         OP_READ(TLOG_RC_MSG_FIELD_INVALID_VALUE, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_in_txt_zero_char,
         MSG_DUMMY(1, 1000, "<3", "X\\u0000Y", "", "", ""),
         4,
         OP_READ_OK(PKT_IO(1, 0, false, "X\0Y", 3))
     );

    TEST_ANY(io_everything,
         MSG_DUMMY(1, 1000, "<1[1/1+234>1]1/1", "1.", "50", "3.", "52"),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "12")),
         OP_READ_OK(PKT_IO_STR(1, 234000000, true, "34"))
    );

    TEST_ANY(id_repeat,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(1, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(id_backwards,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(0, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(id_gap,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(3, 2000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(ts_repeat,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(2, 1000, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ_OK(PKT_WINDOW(1, 0, 210, 220)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(ts_backwards,
         MSG_DUMMY(1, 1000, "=110x120", "", "", "", "")
         MSG_DUMMY(2, 500, "=210x220", "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(1, 0, 110, 120)),
         OP_READ(TLOG_RC_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_ts_null_no_overlap,
         MSG_DUMMY(1, 1000, "", "", "", "", "")
         MSG_DUMMY(2, 1000, "<1", "X", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_ts_hanging_no_overlap,
         MSG_DUMMY(1, 1000, "<1+1", "X", "", "", "")
         MSG_DUMMY(2, 1000, "<1", "Y", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "Y")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_ts_repeat,
         MSG_DUMMY(1, 1000, "<1+1<1", "XY", "", "", "")
         MSG_DUMMY(2, 1000, "<1", "Z", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(1, 1000000, false, "Y")),
         OP_READ(TLOG_RC_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_ts_overlap,
         MSG_DUMMY(1, 1000, "<1+2<1", "XY", "", "", "")
         MSG_DUMMY(2, 1001, "<1", "Z", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(1, 2000000, false, "Y")),
         OP_READ(TLOG_RC_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(io_ts_no_gap,
         MSG_DUMMY(1, 1000, "<1+2<1", "XY", "", "", "")
         MSG_DUMMY(2, 1002, "<1", "Z", "", "", ""),
         4,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "X")),
         OP_READ_OK(PKT_IO_STR(1, 2000000, false, "Y")),
         OP_READ_OK(PKT_IO_STR(1, 2000000, false, "Z")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(delays_and_windows,
         MSG_DUMMY(1, 1000, "+1000=110x120+1000=210x220",
                   "", "", "", ""),
         4,
         OP_READ_OK(PKT_WINDOW(2, 0, 110, 120)),
         OP_READ_OK(PKT_WINDOW(3, 0, 210, 220)),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(window_between_chars,
         MSG_DUMMY(1, 1000, "<1=100x200<1", "AB", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "A")),
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "B")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(window_between_strings,
         MSG_DUMMY(1, 1000, "<3=100x200<3", "ABCXYZ", "", "", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "ABC")),
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_IO_STR(1, 0, false, "XYZ")),
         OP_READ_OK(PKT_VOID)
    );

    TEST_ANY(window_between_streams,
         MSG_DUMMY(1, 1000, "<3=100x200>3", "ABC", "", "XYZ", ""),
         6,
         OP_READ_OK(PKT_IO_STR(1, 0, false, "ABC")),
         OP_READ_OK(PKT_WINDOW(1, 0, 100, 200)),
         OP_READ_OK(PKT_IO_STR(1, 0, true, "XYZ")),
         OP_READ_OK(PKT_VOID)
    );

    return !passed;
}
