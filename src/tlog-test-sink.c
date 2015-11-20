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
#include <limits.h>
#include <tlog/mem_writer.h>
#include <tlog/sink.h>
#include <tlog/misc.h>
#include <tlog/test_misc.h>

#define SIZE    TLOG_SINK_CHUNK_SIZE_MIN

#if __WORDSIZE == 64
#define TIME_T_MAX_NUM (time_t)9223372036854775807
#define TIME_T_MAX_STR "9223372036854775807"
#else
#define TIME_T_MAX_NUM (time_t)2147483647
#define TIME_T_MAX_STR "2147483647"
#endif

#define TIME_MAX_STR TIME_T_MAX_STR "999"

#define USEC_MAX_NUM    999999999

enum op_type {
    OP_TYPE_NONE,
    OP_TYPE_WRITE,
    OP_TYPE_FLUSH,
    OP_TYPE_CUT,
    OP_TYPE_NUM
};

static const char*
op_type_to_str(enum op_type t)
{
    switch (t) {
    case OP_TYPE_NONE:
        return "none";
    case OP_TYPE_WRITE:
        return "write";
    case OP_TYPE_FLUSH:
        return "flush";
    case OP_TYPE_CUT:
        return "cut";
    default:
        return "<unknown>";
    }
}

struct op {
    enum op_type        type;
    union {
        struct tlog_pkt write;
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
    tlog_grc grc;
    struct tlog_writer *writer = NULL;
    struct tlog_sink sink;
    const struct op *op;
    size_t exp_output_len;
    size_t res_output_len;
    char *res_output = NULL;

    grc = tlog_mem_writer_create(&writer, &res_output, &res_output_len);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed creating memory writer: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }

    grc = tlog_sink_init(&sink, writer, t.hostname, t.username,
                         t.session_id, SIZE);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed initializing the sink: %s\n",
                tlog_grc_strerror(grc));
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
        case OP_TYPE_WRITE:
            CHECK_OP(tlog_sink_write(&sink, &op->data.write));
            break;
        case OP_TYPE_FLUSH:
            CHECK_OP(tlog_sink_flush(&sink));
            break;
        case OP_TYPE_CUT:
            CHECK_OP(tlog_sink_cut(&sink));
            break;
        default:
            fprintf(stderr, "Unknown operation type: %d\n", op->type);
            exit(1);
        }
    }

#undef CHECK_OP
#undef FAIL_OP

    exp_output_len = strlen((const char *)t.output);

    if (res_output_len != exp_output_len ||
        memcmp(res_output, t.output, res_output_len) != 0) {
        fprintf(stderr, "%s: output mismatch:\n", n);
        tlog_test_diff(stderr,
                       (const uint8_t *)res_output, res_output_len,
                       (const uint8_t *)t.output, exp_output_len);
        passed = false;
    }
#undef FAIL

    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

cleanup:
    tlog_writer_destroy(writer);
    free(res_output);
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

#define OP_WRITE_WINDOW(_sec, _nsec, _width, _height) \
    {                                                   \
        .type = OP_TYPE_WRITE,                          \
        .data.write = {                                 \
            .type = TLOG_PKT_TYPE_WINDOW,               \
            .timestamp = {                              \
                .tv_sec = _sec,                         \
                .tv_nsec = _nsec                        \
            },                                          \
            .data.window = {                            \
                .width = _width,                        \
                .height = _height                       \
            },                                          \
        }                                               \
    }

#define OP_WRITE_IO(_sec, _nsec, _output, _buf, _len) \
    {                                                   \
        .type = OP_TYPE_WRITE,                          \
        .data.write = {                                 \
            .type = TLOG_PKT_TYPE_IO,                   \
            .timestamp = {                              \
                .tv_sec = _sec,                         \
                .tv_nsec = _nsec                        \
            },                                          \
            .data.io={                                  \
                .output = _output,                      \
                .buf = (uint8_t *)_buf,                 \
                .len = _len,                            \
            }                                           \
        }                                               \
    }

#define OP_FLUSH {.type = OP_TYPE_FLUSH}

#define OP_CUT {.type = OP_TYPE_CUT}

#define MSG(_id_tkn, _pos, _timing, \
            _in_txt, _in_bin, _out_txt, _out_bin)               \
    "{\"host\":\"localhost\","                                  \
      "\"user\":\"user\",\"session\":0,"                        \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","                   \
      "\"timing\":\"" _timing "\","                             \
      "\"in_txt\":\"" _in_txt "\",\"in_bin\":[" _in_bin "],"    \
      "\"out_txt\":\"" _out_txt "\",\"out_bin\":[" _out_bin "]" \
    "}\n"

    TEST(null,              .op_list = {},
                            .output = "");
    TEST(null_cut,          .op_list = {OP_CUT},
                            .output = "");
    TEST(null_flushed,      .op_list = {OP_FLUSH},
                            .output = "");
    TEST(null_cut_flushed,  .op_list = {OP_CUT, OP_FLUSH},
                            .output = "");
    TEST(null_flushed_cut,  .op_list = {OP_FLUSH, OP_CUT},
                            .output = "");

    TEST(window_unflushed,
         .op_list = {OP_WRITE_WINDOW(0, 0, 100, 200)},
         .output = ""
    );

    TEST(window_cut,
         .op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_CUT},
         .output = ""
    );

    TEST(window_flushed,
         .op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_cut_flushed,
         .op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_CUT, OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_flushed_cut,
         .op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_FLUSH, OP_CUT},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(min_window,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 0, 0),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=0x0", "", "", "", "")
    );

    TEST(max_window,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, USHRT_MAX, USHRT_MAX),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=65535x65535", "", "", "", "")
    );

    TEST(three_windows,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_WRITE_WINDOW(0, 0, 300, 300),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100=200x200=300x300", "", "", "", "")
    );

    TEST(delay_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 100000000, 200, 200),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100+100=200x200", "", "", "", "")
    );

    TEST(window_chunk_overflow,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 11111, 11111),
            OP_WRITE_WINDOW(0, 0, 22222, 22222),
            OP_WRITE_WINDOW(0, 0, 33333, 33333),
            OP_WRITE_WINDOW(0, 0, 44444, 44444),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=11111x11111=22222x22222", "", "", "", "")
                   MSG(2, "0", "=33333x33333=44444x44444", "", "", "", "")
    );

    TEST(one_byte,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1)
         },
         .output = ""
    );
    TEST(one_byte_cut,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT
         },
         .output = ""
    );
    TEST(one_byte_flushed,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_flushed_cut,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH,
            OP_CUT
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_cut_flushed,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );

    TEST(input_overflow_flush,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "0123456789abcdef0123456789abcd", 30),
         },
         .output = MSG(1, "0", "<29", 
                       "0123456789abcdef0123456789abc", "",
                       "", "")
    );

    TEST(output_overflow_flush,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "0123456789abcdef0123456789abcd", 30),
         },
         .output = MSG(1, "0", ">29", "", "",
                       "0123456789abcdef0123456789abc", "")
    );

    TEST(both_overflow_flush,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "0123456789abcdef", 16),
            OP_WRITE_IO(0, 0, true, "0123456789a", 11),
         },
         .output = MSG(1, "0", "<16>10", 
                       "0123456789abcdef", "",
                       "0123456789", "")
    );

    TEST(incomplete,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3)
         },
         .output = ""
    );

    TEST(incomplete_flushed,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_FLUSH
         },
         .output = ""
    );

    TEST(incomplete_cut,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT
         },
         .output = ""
    );

    TEST(incomplete_cut_flushed,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(split_valid,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(split_before_invalid,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84X", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_on_invalid,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_WRITE_IO(0, 0, true, "X", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_incomplete,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84", 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(delay_inside_char,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 1100000, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "+1>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_between_chars,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+1>1", "A", "", "B", "")
    );

    TEST(delay_between_messages,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, "1", ">1", "", "", "B", "")
    );

    TEST(max_delay_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(TIME_T_MAX_NUM, USEC_MAX_NUM, 200, 200),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x100", "", "", "", "")
                   MSG(2, TIME_MAX_STR, "=200x200", "", "", "", "")
    );

    TEST(max_delay_inside_char,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(TIME_T_MAX_NUM, USEC_MAX_NUM, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "+" TIME_MAX_STR ">1",
                       "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_between_chars,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(TIME_T_MAX_NUM, USEC_MAX_NUM, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+" TIME_MAX_STR ">1",
                       "A", "", "B", "")
    );

    TEST(max_delay_between_messages,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(TIME_T_MAX_NUM, USEC_MAX_NUM, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, TIME_MAX_STR, ">1", "", "", "B", "")
    );

    TEST(window_between_chars,
         .op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1=100x200>1", "A", "", "B", "")
    );

    TEST(char_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x100<1=200x200", "A", "", "", "")
    );

    TEST(window_inside_char,
         .op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x200>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(windows_chars_and_delays,
         .op_list = {
            OP_WRITE_IO(0, 10000000, true, "S", 1),
            OP_WRITE_WINDOW(0, 30000000, 100, 100),
            OP_WRITE_WINDOW(0, 60000000, 200, 200),
            OP_WRITE_IO(0, 100000000, true, "E", 1),
            OP_FLUSH,
         },
         .output = MSG(1, "0", ">1+20=100x100+30=200x200+40>1", "", "", "SE", "")
    );

    return !passed;
}
