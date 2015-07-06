/*
 * Tlog tlog_sink test.
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
#include "tlog_sink.h"
#include "tlog_misc.h"
#include "tlog_test.h"

#define SIZE    TLOG_SINK_IO_SIZE_MIN

#if __WORDSIZE == 64
#define TIME_T_MAX_NUM (time_t)9223372036854775807
#define TIME_T_MAX_STR "9223372036854775807"
#else
#define TIME_T_MAX_NUM (time_t)2147483647
#define TIME_T_MAX_STR "2147483647"
#endif

#define POS_MAX_STR TIME_T_MAX_STR ".999"
#define OFF_MAX_STR TIME_T_MAX_STR "999"

enum op_type {
    OP_TYPE_NONE,
    OP_TYPE_WINDOW_WRITE,
    OP_TYPE_IO_WRITE,
    OP_TYPE_IO_FLUSH,
    OP_TYPE_IO_CUT,
    OP_TYPE_DELAY,
    OP_TYPE_NUM
};

static const char*
op_type_to_str(enum op_type t)
{
    switch (t) {
        case OP_TYPE_NONE:
            return "none";
        case OP_TYPE_WINDOW_WRITE:
            return "window_write";
        case OP_TYPE_IO_WRITE:
            return "io_write";
        case OP_TYPE_IO_FLUSH:
            return "io_flush";
        case OP_TYPE_IO_CUT:
            return "io_cut";
        case OP_TYPE_DELAY:
            return "delay";
        default:
            return "<unkown>";
    }
}

struct op_data_window_write {
    unsigned short int width;
    unsigned short int height;
};

struct op_data_io_write {
    bool        output;
    uint8_t     buf[SIZE];
    size_t      len;
};

struct op {
    enum op_type type;
    union {
        struct op_data_window_write window_write;
        struct op_data_io_write     io_write;
        struct timespec             delay;
    } data;
};

struct test {
    const char     *hostname;
    const char     *username;
    unsigned int    session_id;
    struct op       op_list[16];
    const char     *output;
};

static bool
test(const char *n, const struct test t)
{
    bool passed = true;
    int fd = -1;
    char filename[] = "tlog_sink_test.XXXXXX";
    struct timespec timestamp = {1, 0}; /* Must be non-zero */
    struct tlog_sink sink;
    const struct op *op;
    off_t end;
    size_t exp_output_len;
    size_t res_output_len;
    size_t max_output_len;
    char *res_output = NULL;
    char *exp_output = NULL;

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

    if (tlog_sink_init(&sink, fd, t.hostname, t.username,
                       t.session_id, SIZE, &timestamp) != TLOG_RC_OK) {
        fprintf(stderr, "Failed initializing the sink: %s\n",
                strerror(errno));
        exit(1);
    };

#define FAIL(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, "%s: " _fmt "\n", n, ##_args);  \
        passed = false;                                 \
    } while (0)

#define FAIL_OP \
    do {                                                    \
        FAIL("op #%zd (%s) failed",                         \
             op - t.op_list + 1, op_type_to_str(op->type)); \
        goto cleanup;                                       \
    } while (0)

#define CHECK_OP(_expr) \
    do {                            \
        if ((_expr) != TLOG_RC_OK)  \
            FAIL_OP;                \
    } while (0)

    for (op = t.op_list; op->type != OP_TYPE_NONE; op++) {
        switch (op->type) {
            case OP_TYPE_WINDOW_WRITE:
                CHECK_OP(tlog_sink_window_write(
                            &sink, &timestamp,
                            op->data.window_write.width,
                            op->data.window_write.height));
                break;
            case OP_TYPE_IO_WRITE:
                CHECK_OP(tlog_sink_io_write(
                            &sink, &timestamp,
                            op->data.io_write.output,
                            op->data.io_write.buf,
                            op->data.io_write.len));
                break;
            case OP_TYPE_IO_FLUSH:
                CHECK_OP(tlog_sink_io_flush(&sink));
                break;
            case OP_TYPE_IO_CUT:
                CHECK_OP(tlog_sink_io_cut(&sink, &timestamp));
                break;
            case OP_TYPE_DELAY:
                tlog_timespec_add(&timestamp, &op->data.delay, &timestamp);
                break;
            default:
                fprintf(stderr, "Unknown operation type: %d\n", op->type);
                exit(1);
        }
    }

#undef CHECK_OP
#undef FAIL_OP

    end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        fprintf(stderr, "Failed to seek the file: %s\n", strerror(errno));
        exit(1);
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "Failed to seek the file: %s\n", strerror(errno));
        exit(1);
    }

    res_output_len = (size_t)end;
    exp_output_len = strlen((const char *)t.output);
    if (res_output_len != exp_output_len)
        FAIL("len %zu != %zu", res_output_len, exp_output_len);
    max_output_len = TLOG_MAX(res_output_len, exp_output_len);


    res_output = calloc(max_output_len, 1);
    exp_output = calloc(max_output_len, 1);
    if (res_output == NULL || exp_output == NULL) {
        fprintf(stderr, "Failed to allocate output buffers: %s\n", strerror(errno));
        exit(1);
    }

    if (read(fd, res_output, res_output_len) < 0) {
        fprintf(stderr, "Failed to read the file: %s\n", strerror(errno));
        exit(1);
    }
    memcpy(exp_output, t.output, exp_output_len);

    if (memcmp(res_output, t.output, max_output_len) != 0) {
        fprintf(stderr, "%s: output mismatch:\n", n);
        tlog_test_diff(stderr,
                       (const uint8_t *)res_output,
                       (const uint8_t *)exp_output,
                       max_output_len);
        passed = false;
    }
#undef FAIL

    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

cleanup:
    free(exp_output);
    free(res_output);
    if (fd >= 0)
        close(fd);
    tlog_sink_cleanup(&sink);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = test(#_name_token,                 \
                  (struct test){                \
                    .hostname = "localhost",    \
                    .username = "user",         \
                    .session_id = 0,            \
                    _struct_init_args           \
                  }                             \
                 ) && passed

#define OP_NONE {.type = OP_TYPE_NONE}

#define OP_WINDOW_WRITE(_data_init_args...) \
    {.type = OP_TYPE_WINDOW_WRITE,                  \
     .data = {.window_write = {_data_init_args}}}

#define OP_IO_WRITE(_data_init_args...) \
    {.type = OP_TYPE_IO_WRITE,                  \
     .data = {.io_write = {_data_init_args}}}

#define OP_IO_FLUSH {.type = OP_TYPE_IO_FLUSH}

#define OP_IO_CUT {.type = OP_TYPE_IO_CUT}

#define OP_DELAY(_data_init_args...) \
    {.type = OP_TYPE_DELAY,                 \
     .data = {.delay = {_data_init_args}}}

#define OP_DELAY_MAX \
    OP_DELAY(.tv_sec = TIME_T_MAX_NUM, .tv_nsec = 999999999)

#define MSG_WINDOW(_id_tkn, _pos, _width_tkn, _height_tkn) \
    "{\"type\":\"window\",\"host\":\"localhost\","          \
      "\"user\":\"user\",\"session\":0,"                    \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","               \
      "\"width\":" #_width_tkn ",\"height\":" #_height_tkn  \
    "}\n"

#define MSG_IO(_id_tkn, _pos, _timing, \
               _in_txt, _in_bin, _out_txt, _out_bin)            \
    "{\"type\":\"io\",\"host\":\"localhost\","                  \
      "\"user\":\"user\",\"session\":0,"                        \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","                   \
      "\"timing\":\"" _timing "\","                             \
      "\"in_txt\":\"" _in_txt "\",\"in_bin\":[" _in_bin "],"    \
      "\"out_txt\":\"" _out_txt "\",\"out_bin\":[" _out_bin "]" \
    "}\n"

    TEST(null,              .op_list = {OP_NONE},
                            .output = "");
    TEST(null_cut,          .op_list = {OP_IO_CUT},
                            .output = "");
    TEST(null_flushed,      .op_list = {OP_IO_FLUSH},
                            .output = "");
    TEST(null_cut_flushed,  .op_list = {OP_IO_CUT, OP_IO_FLUSH},
                            .output = "");
    TEST(null_flushed_cut,  .op_list = {OP_IO_FLUSH, OP_IO_CUT},
                            .output = "");

    TEST(window,
         .op_list = {OP_WINDOW_WRITE(.width = 256, .height = 256)},
         .output = MSG_WINDOW(0, "0.000", 256, 256)
    );

    TEST(three_windows,
         .op_list = {
            OP_WINDOW_WRITE(.width = 100, .height = 100),
            OP_WINDOW_WRITE(.width = 200, .height = 200),
            OP_WINDOW_WRITE(.width = 300, .height = 300),
         },
         .output = MSG_WINDOW(0, "0.000", 100, 100) \
                   MSG_WINDOW(1, "0.000", 200, 200) \
                   MSG_WINDOW(2, "0.000", 300, 300)
    );

    TEST(one_byte,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {'A'}, .len = 1)
         },
         .output = ""
    );
    TEST(one_byte_cut,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {'A'}, .len = 1),
            OP_IO_CUT
         },
         .output = ""
    );
    TEST(one_byte_flushed,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {'A'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "A", "")
    );
    TEST(one_byte_flushed_cut,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {'A'}, .len = 1),
            OP_IO_FLUSH,
            OP_IO_CUT
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "A", "")
    );
    TEST(one_byte_cut_flushed,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {'A'}, .len = 1),
            OP_IO_CUT,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "A", "")
    );

    TEST(input_overflow_flush,
         .op_list = {
            OP_IO_WRITE(.output = false,
                        .buf = "0123456789abcdef0123456789abcd",
                        .len = 30),
         },
         .output = MSG_IO(0, "0.000", "<29", 
                          "0123456789abcdef0123456789abc", "",
                          "", "")
    );

    TEST(output_overflow_flush,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = "0123456789abcdef0123456789abcd",
                        .len = 30),
         },
         .output = MSG_IO(0, "0.000", ">29", "", "",
                          "0123456789abcdef0123456789abc", "")
    );

    TEST(both_overflow_flush,
         .op_list = {
            OP_IO_WRITE(.output = false,
                        .buf = "0123456789abcdef",
                        .len = 16),
            OP_IO_WRITE(.output = true,
                        .buf = "0123456789a",
                        .len = 11),
         },
         .output = MSG_IO(0, "0.000", "<16>10", 
                          "0123456789abcdef", "",
                          "0123456789", "")
    );

    TEST(incomplete,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3)
         },
         .output = ""
    );

    TEST(incomplete_flushed,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_IO_FLUSH
         },
         .output = ""
    );

    TEST(incomplete_cut,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_IO_CUT
         },
         .output = ""
    );

    TEST(incomplete_cut_flushed,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_IO_CUT,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(split_valid,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_IO_WRITE(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(split_before_invalid,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_IO_WRITE(.output = true, .buf = {0x84, 'X'}, .len = 2),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_on_invalid,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d, 0x84}, .len = 3),
            OP_IO_WRITE(.output = true, .buf = {'X'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_incomplete,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_IO_WRITE(.output = true, .buf = {0x84}, .len = 1),
            OP_IO_CUT,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(delay_before,
         .op_list = {
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.001", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_after,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_before_cut,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e, 0xf0},
                        .len = 5),
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_CUT,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1+1]1/1",
                          "", "", "\xf0\x9d\x84\x9e�", "240")
    );

    TEST(delay_inside_char,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_WRITE(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "+1>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_between_chars,
         .op_list = {
            OP_IO_WRITE(.output = false, .buf = {'A'}, .len = 1),
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_WRITE(.output = true, .buf = {'B'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "<1+1>1", "A", "", "B", "")
    );

    TEST(delay_between_messages,
         .op_list = {
            OP_IO_WRITE(.output = false, .buf = {'A'}, .len = 1),
            OP_IO_FLUSH,
            OP_DELAY(.tv_nsec = 1100000),
            OP_IO_WRITE(.output = true, .buf = {'B'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "<1", "A", "", "", "") \
                   MSG_IO(1, "0.001", ">1", "", "", "B", "")
    );

    TEST(max_delay_between_windows,
         .op_list = {
            OP_WINDOW_WRITE(.width = 100, .height = 100),
            OP_DELAY_MAX,
            OP_WINDOW_WRITE(.width = 200, .height = 200)
         },
         .output = MSG_WINDOW(0, "0.000", 100, 100) \
                   MSG_WINDOW(1, POS_MAX_STR, 200, 200)
    );

    TEST(max_delay_before,
         .op_list = {
            OP_DELAY_MAX,
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, POS_MAX_STR, ">1",
                          "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_after,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_DELAY_MAX,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_before_cut,
         .op_list = {
            OP_IO_WRITE(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e, 0xf0},
                        .len = 5),
            OP_DELAY_MAX,
            OP_IO_CUT,
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", ">1", "", "", "\xf0\x9d\x84\x9e", "")
                   MSG_IO(1, POS_MAX_STR, "]1/1", "", "", "�", "240")
    );

    TEST(max_delay_inside_char,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_DELAY_MAX,
            OP_IO_WRITE(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "+" OFF_MAX_STR ">1",
                          "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_between_chars,
         .op_list = {
            OP_IO_WRITE(.output = false, .buf = {'A'}, .len = 1),
            OP_DELAY_MAX,
            OP_IO_WRITE(.output = true, .buf = {'B'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "<1+" OFF_MAX_STR ">1",
                          "A", "", "B", "")
    );

    TEST(max_delay_between_messages,
         .op_list = {
            OP_IO_WRITE(.output = false, .buf = {'A'}, .len = 1),
            OP_IO_FLUSH,
            OP_DELAY_MAX,
            OP_IO_WRITE(.output = true, .buf = {'B'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "<1", "A", "", "", "") \
                   MSG_IO(1, POS_MAX_STR, ">1", "", "", "B", "")
    );

    TEST(window_between_chars,
         .op_list = {
            OP_IO_WRITE(.output = false, .buf = {'A'}, .len = 1),
            OP_WINDOW_WRITE(.width = 256, .height = 256),
            OP_IO_WRITE(.output = true, .buf = {'B'}, .len = 1),
            OP_IO_FLUSH
         },
         .output = MSG_IO(0, "0.000", "<1", "A", "", "", "") \
                   MSG_WINDOW(1, "0.000", 256, 256) \
                   MSG_IO(2, "0.000", ">1", "", "", "B", "")
    );

    TEST(window_inside_char,
         .op_list = {
            OP_IO_WRITE(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_WINDOW_WRITE(.width = 256, .height = 256),
            OP_IO_WRITE(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_IO_FLUSH
         },
         .output = MSG_WINDOW(0, "0.000", 256, 256) \
                   MSG_IO(1, "0.000", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    return !passed;
}
