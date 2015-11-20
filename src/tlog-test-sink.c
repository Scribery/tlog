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

enum op_type {
    OP_TYPE_NONE,
    OP_TYPE_WRITE_WINDOW,
    OP_TYPE_WRITE_IO,
    OP_TYPE_FLUSH,
    OP_TYPE_CUT,
    OP_TYPE_DELAY,
    OP_TYPE_NUM
};

static const char*
op_type_to_str(enum op_type t)
{
    switch (t) {
    case OP_TYPE_NONE:
        return "none";
    case OP_TYPE_WRITE_WINDOW:
        return "write_window";
    case OP_TYPE_WRITE_IO:
        return "write_io";
    case OP_TYPE_FLUSH:
        return "flush";
    case OP_TYPE_CUT:
        return "cut";
    case OP_TYPE_DELAY:
        return "delay";
    default:
        return "<unknown>";
    }
}

struct op_data_write_window {
    unsigned short int width;
    unsigned short int height;
};

struct op_data_write_io {
    bool        output;
    uint8_t     buf[SIZE];
    size_t      len;
};

struct op {
    enum op_type type;
    union {
        struct op_data_write_window write_window;
        struct op_data_write_io     write_io;
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
    tlog_grc grc;
    struct tlog_writer *writer = NULL;
    struct tlog_pkt pkt = TLOG_PKT_VOID;
    struct timespec timestamp = TLOG_TIMESPEC_ZERO;
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
        case OP_TYPE_WRITE_WINDOW:
            tlog_pkt_init_window(&pkt,
                                 &timestamp,
                                 op->data.write_window.width,
                                 op->data.write_window.height);
            CHECK_OP(tlog_sink_write(&sink, &pkt));
            tlog_pkt_cleanup(&pkt);
            break;
        case OP_TYPE_WRITE_IO:
            tlog_pkt_init_io(&pkt,
                             &timestamp,
                             op->data.write_io.output,
                             (uint8_t *)op->data.write_io.buf,
                             false,
                             op->data.write_io.len);
            CHECK_OP(tlog_sink_write(&sink, &pkt));
            tlog_pkt_cleanup(&pkt);
            break;
        case OP_TYPE_FLUSH:
            CHECK_OP(tlog_sink_flush(&sink));
            break;
        case OP_TYPE_CUT:
            CHECK_OP(tlog_sink_cut(&sink));
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
    tlog_pkt_cleanup(&pkt);
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

#define OP_WRITE_WINDOW(_data_init_args...) \
    {.type = OP_TYPE_WRITE_WINDOW,                  \
     .data = {.write_window = {_data_init_args}}}

#define OP_WRITE_IO(_data_init_args...) \
    {.type = OP_TYPE_WRITE_IO,                  \
     .data = {.write_io = {_data_init_args}}}

#define OP_FLUSH {.type = OP_TYPE_FLUSH}

#define OP_CUT {.type = OP_TYPE_CUT}

#define OP_DELAY(_data_init_args...) \
    {.type = OP_TYPE_DELAY,                 \
     .data = {.delay = {_data_init_args}}}

#define OP_DELAY_MAX \
    OP_DELAY(.tv_sec = TIME_T_MAX_NUM, .tv_nsec = 999999999)

#define MSG(_id_tkn, _pos, _timing, \
            _in_txt, _in_bin, _out_txt, _out_bin)               \
    "{\"host\":\"localhost\","                                  \
      "\"user\":\"user\",\"session\":0,"                        \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","                   \
      "\"timing\":\"" _timing "\","                             \
      "\"in_txt\":\"" _in_txt "\",\"in_bin\":[" _in_bin "],"    \
      "\"out_txt\":\"" _out_txt "\",\"out_bin\":[" _out_bin "]" \
    "}\n"

    TEST(null,              .op_list = {OP_NONE},
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
         .op_list = {OP_WRITE_WINDOW(.width = 100, .height = 200)},
         .output = ""
    );

    TEST(window_cut,
         .op_list = {OP_WRITE_WINDOW(.width = 100, .height = 200), OP_CUT},
         .output = ""
    );

    TEST(window_flushed,
         .op_list = {OP_WRITE_WINDOW(.width = 100, .height = 200), OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_cut_flushed,
         .op_list = {OP_WRITE_WINDOW(.width = 100, .height = 200), OP_CUT, OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_flushed_cut,
         .op_list = {OP_WRITE_WINDOW(.width = 100, .height = 200), OP_FLUSH, OP_CUT},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(min_window,
         .op_list = {
            OP_WRITE_WINDOW(.width = 0, .height = 0),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=0x0", "", "", "", "")
    );

    TEST(max_window,
         .op_list = {
            OP_WRITE_WINDOW(.width = USHRT_MAX, .height = USHRT_MAX),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=65535x65535", "", "", "", "")
    );

    TEST(three_windows,
         .op_list = {
            OP_WRITE_WINDOW(.width = 100, .height = 100),
            OP_WRITE_WINDOW(.width = 200, .height = 200),
            OP_WRITE_WINDOW(.width = 300, .height = 300),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100=200x200=300x300", "", "", "", "")
    );

    TEST(delay_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(.width = 100, .height = 100),
            OP_DELAY(.tv_nsec = 100000000),
            OP_WRITE_WINDOW(.width = 200, .height = 200),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100+100=200x200", "", "", "", "")
    );

    TEST(window_chunk_overflow,
         .op_list = {
            OP_WRITE_WINDOW(.width = 11111, .height = 11111),
            OP_WRITE_WINDOW(.width = 22222, .height = 22222),
            OP_WRITE_WINDOW(.width = 33333, .height = 33333),
            OP_WRITE_WINDOW(.width = 44444, .height = 44444),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=11111x11111=22222x22222", "", "", "", "")
                   MSG(2, "0", "=33333x33333=44444x44444", "", "", "", "")
    );

    TEST(one_byte,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {'A'}, .len = 1)
         },
         .output = ""
    );
    TEST(one_byte_cut,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {'A'}, .len = 1),
            OP_CUT
         },
         .output = ""
    );
    TEST(one_byte_flushed,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {'A'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_flushed_cut,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {'A'}, .len = 1),
            OP_FLUSH,
            OP_CUT
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_cut_flushed,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {'A'}, .len = 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );

    TEST(input_overflow_flush,
         .op_list = {
            OP_WRITE_IO(.output = false,
                        .buf = "0123456789abcdef0123456789abcd",
                        .len = 30),
         },
         .output = MSG(1, "0", "<29", 
                       "0123456789abcdef0123456789abc", "",
                       "", "")
    );

    TEST(output_overflow_flush,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = "0123456789abcdef0123456789abcd",
                        .len = 30),
         },
         .output = MSG(1, "0", ">29", "", "",
                       "0123456789abcdef0123456789abc", "")
    );

    TEST(both_overflow_flush,
         .op_list = {
            OP_WRITE_IO(.output = false,
                        .buf = "0123456789abcdef",
                        .len = 16),
            OP_WRITE_IO(.output = true,
                        .buf = "0123456789a",
                        .len = 11),
         },
         .output = MSG(1, "0", "<16>10", 
                       "0123456789abcdef", "",
                       "0123456789", "")
    );

    TEST(incomplete,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3)
         },
         .output = ""
    );

    TEST(incomplete_flushed,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_FLUSH
         },
         .output = ""
    );

    TEST(incomplete_cut,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_CUT
         },
         .output = ""
    );

    TEST(incomplete_cut_flushed,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84},
                        .len = 3),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(split_valid,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_WRITE_IO(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(split_before_invalid,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_WRITE_IO(.output = true, .buf = {0x84, 'X'}, .len = 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_on_invalid,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d, 0x84}, .len = 3),
            OP_WRITE_IO(.output = true, .buf = {'X'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_incomplete,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_WRITE_IO(.output = true, .buf = {0x84}, .len = 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(delay_after,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_DELAY(.tv_nsec = 1100000),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_before_cut,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e, 0xf0},
                        .len = 5),
            OP_DELAY(.tv_nsec = 1100000),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1]1/1",
                       "", "", "\xf0\x9d\x84\x9e�", "240")
    );

    TEST(delay_inside_char,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_DELAY(.tv_nsec = 1100000),
            OP_WRITE_IO(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "+1>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_between_chars,
         .op_list = {
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_DELAY(.tv_nsec = 1100000),
            OP_WRITE_IO(.output = true, .buf = {'B'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+1>1", "A", "", "B", "")
    );

    TEST(delay_between_messages,
         .op_list = {
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_FLUSH,
            OP_DELAY(.tv_nsec = 1100000),
            OP_WRITE_IO(.output = true, .buf = {'B'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, "1", ">1", "", "", "B", "")
    );

    TEST(max_delay_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(.width = 100, .height = 100),
            OP_DELAY_MAX,
            OP_WRITE_WINDOW(.width = 200, .height = 200),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x100", "", "", "", "")
                   MSG(2, TIME_MAX_STR, "=200x200", "", "", "", "")
    );

    TEST(max_delay_after,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e},
                        .len = 4),
            OP_DELAY_MAX,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_before_cut,
         .op_list = {
            OP_WRITE_IO(.output = true,
                        .buf = {0xf0, 0x9d, 0x84, 0x9e, 0xf0},
                        .len = 5),
            OP_DELAY_MAX,
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1]1/1",
                       "", "", "\xf0\x9d\x84\x9e�", "240")
    );

    TEST(max_delay_inside_char,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_DELAY_MAX,
            OP_WRITE_IO(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "+" TIME_MAX_STR ">1",
                       "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_between_chars,
         .op_list = {
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_DELAY_MAX,
            OP_WRITE_IO(.output = true, .buf = {'B'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+" TIME_MAX_STR ">1",
                       "A", "", "B", "")
    );

    TEST(max_delay_between_messages,
         .op_list = {
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_FLUSH,
            OP_DELAY_MAX,
            OP_WRITE_IO(.output = true, .buf = {'B'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, TIME_MAX_STR, ">1", "", "", "B", "")
    );

    TEST(window_between_chars,
         .op_list = {
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_WRITE_WINDOW(.width = 100, .height = 200),
            OP_WRITE_IO(.output = true, .buf = {'B'}, .len = 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1=100x200>1", "A", "", "B", "")
    );

    TEST(char_between_windows,
         .op_list = {
            OP_WRITE_WINDOW(.width = 100, .height = 100),
            OP_WRITE_IO(.output = false, .buf = {'A'}, .len = 1),
            OP_WRITE_WINDOW(.width = 200, .height = 200),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x100<1=200x200", "A", "", "", "")
    );

    TEST(window_inside_char,
         .op_list = {
            OP_WRITE_IO(.output = true, .buf = {0xf0, 0x9d}, .len = 2),
            OP_WRITE_WINDOW(.width = 100, .height = 200),
            OP_WRITE_IO(.output = true, .buf = {0x84, 0x9e}, .len = 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x200>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(windows_chars_and_delays,
         .op_list = {
            OP_DELAY(.tv_nsec = 10000000),
            OP_WRITE_IO(.output = true, .buf = {'S'}, .len = 1),
            OP_DELAY(.tv_nsec = 20000000),
            OP_WRITE_WINDOW(.width = 100, .height = 100),
            OP_DELAY(.tv_nsec = 30000000),
            OP_WRITE_WINDOW(.width = 200, .height = 200),
            OP_DELAY(.tv_nsec = 40000000),
            OP_WRITE_IO(.output = true, .buf = {'E'}, .len = 1),
            OP_DELAY(.tv_nsec = 50000000),
            OP_FLUSH,
         },
         .output = MSG(1, "0", ">1+20=100x100+30=200x200+40>1", "", "", "SE", "")
    );

    return !passed;
}
