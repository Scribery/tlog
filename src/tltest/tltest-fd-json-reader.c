/*
 * Tlog tlog_fd_json_reader test.
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
#include <tlog/fd_json_reader.h>
#include <tlog/misc.h>
#include <tltest/misc.h>

/** Fd reader text buffer size */
#define BUF_SIZE 16

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
    int         exp_grc;
    char       *exp_string;
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
    struct op       op_list[16];
};

static bool
test(const char *file, int line, const char *n, const struct test t)
{
    bool passed = true;
    int fd = -1;
    tlog_grc grc;
    struct tlog_json_reader *reader = NULL;
    char filename[] = "tlog-test-fd-json-reader.XXXXXX";
    const struct op *op;
    struct json_object *object = NULL;
    size_t input_len = strlen(t.input);
    size_t exp_string_len;
    size_t res_string_len;
    const char *res_string;
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
    grc = tlog_fd_json_reader_create(&reader, fd, false, BUF_SIZE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating FD reader: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }

#define FAIL(_fmt, _args...) \
    do {                                              \
        fprintf(stderr, "FAIL %s:%d %s " _fmt "\n",   \
                file, line, n, ##_args);              \
        passed = false;                               \
    } while (0)

#define FAIL_OP(_fmt, _args...) \
    FAIL("op #%zd (%s): " _fmt,                                 \
         op - t.op_list + 1, op_type_to_str(op->type), ##_args)

    for (op = t.op_list; op->type != OP_TYPE_NONE; op++) {
        switch (op->type) {
        case OP_TYPE_READ:
            grc = tlog_json_reader_read(reader, &object);
            if (grc != op->data.read.exp_grc) {
                const char *res_str;
                const char *exp_str;
                res_str = tlog_grc_strerror(grc);
                exp_str = tlog_grc_strerror(op->data.read.exp_grc);
                FAIL_OP("grc: %s (%d) != %s (%d)",
                        res_str, grc,
                        exp_str, op->data.read.exp_grc);
            }
            if ((object == NULL) != (op->data.read.exp_string == NULL)) {
                FAIL_OP("object: %s != %s",
                        (object ? "!NULL" : "NULL"),
                        (op->data.read.exp_string ? "!NULL" : "NULL"));
            }
            else if (object != NULL) {
                res_string = json_object_to_json_string(object);
                res_string_len = strlen(res_string);
                exp_string_len = strlen(op->data.read.exp_string);
                if (res_string_len != exp_string_len ||
                    memcmp(res_string, op->data.read.exp_string,
                           res_string_len) != 0) {
                    FAIL_OP("object mismatch:");
                    tltest_diff(
                            stderr,
                            (const uint8_t *)res_string,
                            res_string_len,
                            (const uint8_t *)op->data.read.exp_string,
                            exp_string_len);
                }
            }
            if (object != NULL) {
                json_object_put(object);
            }
            break;
        case OP_TYPE_LOC_GET:
            loc = tlog_json_reader_loc_get(reader);
            if (loc != op->data.loc_get.exp_loc) {
                char *res_str;
                char *exp_str;
                res_str = tlog_json_reader_loc_fmt(reader, loc);
                exp_str = tlog_json_reader_loc_fmt(reader,
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

    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, n);

    tlog_json_reader_destroy(reader);
    if (fd >= 0) {
        close(fd);
    }
    return passed;
}

int
main(void)
{
    bool passed = true;

#define OP_NONE {.type = OP_TYPE_NONE}

#define OP_READ(_exp_grc, _exp_string) \
    {.type = OP_TYPE_READ,                          \
     .data = {.read = {.exp_grc = _exp_grc,         \
                       .exp_string = _exp_string}}}

#define OP_LOC_GET(_exp_loc) \
    {.type = OP_TYPE_LOC_GET,                       \
     .data = {.loc_get = {.exp_loc = _exp_loc}}}

#define TEST(_name_token, _input, _op_list_init_args...) \
    passed = test(__FILE__, __LINE__, #_name_token,             \
                  (struct test){                                \
                    .input = _input,                            \
                    .op_list = {_op_list_init_args, OP_NONE}    \
                  }                                             \
                 ) && passed


    TEST(null,
         "",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(null_repeat_eof,
         "",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(single_space,
         " ",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(single_space_repeat_eof,
         " ",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(two_spaces,
         " ",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(empty_line,
         "\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(single_space_line,
         " \n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(two_single_space_lines,
         " \n \n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(3));

    TEST(empty_object,
         "{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(empty_object_repeat_eof,
         "{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(empty_object_space_pad_before,
         " {}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(1));

    TEST(empty_object_space_pad_after,
         "{} ",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(1));

    TEST(empty_object_space_pad_both,
         " {} ",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(1));

    TEST(empty_object_newline_pad_before,
         "\n{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2));

    TEST(empty_object_newline_pad_after,
         "{}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2));

    TEST(empty_object_newline_pad_both,
         "\n{}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(3));

    TEST(two_empty_objects_hanging,
         "{}\n{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(two_empty_objects_complete,
         "{}\n{}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(3),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(3));

    TEST(two_empty_objects_apart,
         "{}\n  \n{}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(4),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(4));

    TEST(one_deep_object,
         "{\"x\": 1}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"x\": 1 }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(two_deep_object,
         "{\"x\": [1]}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"x\": [ 1 ] }"),
         OP_LOC_GET(1));

    TEST(object_after_syntax_err,
         "{\"x\": a, \"1\": 2}\n{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                 NULL),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2));

    TEST(eof_after_err,
         "{\"x\": a, \"1\": 2}\n{}",
         OP_LOC_GET(1),
         OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                 NULL),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(premature_eof,
         "{\"x\": 1",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_FD_JSON_READER_INCOMPLETE_LINE, NULL),
         OP_LOC_GET(1));

    TEST(premature_newline,
         "{\"x\": 1\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_FD_JSON_READER_INCOMPLETE_LINE, NULL),
         OP_LOC_GET(2));

    TEST(buf_one_under,
         "{\"abcdefghijklmnopqrstuvwxy\":1}",
         OP_READ(TLOG_RC_OK, "{ \"abcdefghijklmnopqrstuvwxy\": 1 }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(buf_exact,
         "{\"abcdefghijklmnopqrstuvwxyz\":1}",
         OP_READ(TLOG_RC_OK, "{ \"abcdefghijklmnopqrstuvwxyz\": 1 }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(buf_one_over,
         "{\"abcdefghijklmnopqrstuvwxyz\": 1}",
         OP_READ(TLOG_RC_OK, "{ \"abcdefghijklmnopqrstuvwxyz\": 1 }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    TEST(buf_newline_over,
         "{\"abcdefghijklmnopqrstuvwxyz\":1}\n",
         OP_READ(TLOG_RC_OK, "{ \"abcdefghijklmnopqrstuvwxyz\": 1 }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(buf_split_exact,
         "{\"aaaaaaaaaaaaaaaaaaaaaaaaa\":1}\n"
         "{\"bbbbbbbbbbbbbbbbbbbbbbbbb\":2}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"aaaaaaaaaaaaaaaaaaaaaaaaa\": 1 }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ \"bbbbbbbbbbbbbbbbbbbbbbbbb\": 2 }"),
         OP_LOC_GET(3),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(3));

    TEST(buf_split_one_under,
         "{\"aaaaaaaaaaaaaaaaaaaaaaaa\":1}\n"
         "{\"bbbbbbbbbbbbbbbbbbbbbbbbbb\":2}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"aaaaaaaaaaaaaaaaaaaaaaaa\": 1 }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ \"bbbbbbbbbbbbbbbbbbbbbbbbbb\": 2 }"),
         OP_LOC_GET(3),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(3));

    TEST(buf_split_one_over,
         "{\"aaaaaaaaaaaaaaaaaaaaaaaaaa\":1}\n"
         "{\"bbbbbbbbbbbbbbbbbbbbbbbb\":2}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"aaaaaaaaaaaaaaaaaaaaaaaaaa\": 1 }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, "{ \"bbbbbbbbbbbbbbbbbbbbbbbb\": 2 }"),
         OP_LOC_GET(3),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(3));

    TEST(two_objects_one_line,
         "{\"x\":1}{\"y\":2}\n",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, "{ \"x\": 1 }"),
         OP_LOC_GET(2),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(2));

    TEST(multiproperty_object,
         "{\"abc\": 123, \"def\": 456, \"ghi\": 789, "
         "\"bool\": true, \"string\": \"wool\"}",
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK,
                 "{ \"abc\": 123, \"def\": 456, \"ghi\": 789, "
                 "\"bool\": true, \"string\": \"wool\" }"),
         OP_LOC_GET(1),
         OP_READ(TLOG_RC_OK, NULL),
         OP_LOC_GET(1));

    return !passed;
}
