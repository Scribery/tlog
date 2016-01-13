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

#include <tlog/test_sink.h>
#include <tlog/misc.h>
#include <limits.h>

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = tlog_test_sink(                        \
                #_name_token,                       \
                (struct tlog_test_sink){            \
                    _struct_init_args,              \
                    .input.hostname = "localhost",  \
                    .input.username = "user",       \
                    .input.session_id = 0,          \
                }                                   \
    ) && passed

#define OP_NONE {.type = OP_TYPE_NONE}

#define OP_WRITE_WINDOW(_sec, _nsec, _width, _height) \
    {                                                   \
        .type = TLOG_TEST_SINK_OP_TYPE_WRITE,           \
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
        .type = TLOG_TEST_SINK_OP_TYPE_WRITE,           \
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

#define OP_FLUSH {.type = TLOG_TEST_SINK_OP_TYPE_FLUSH}

#define OP_CUT {.type = TLOG_TEST_SINK_OP_TYPE_CUT}

#define MSG(_id_tkn, _pos, _timing, \
            _in_txt, _in_bin, _out_txt, _out_bin)               \
    "{\"host\":\"localhost\","                                  \
      "\"user\":\"user\",\"session\":0,"                        \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","                   \
      "\"timing\":\"" _timing "\","                             \
      "\"in_txt\":\"" _in_txt "\",\"in_bin\":[" _in_bin "],"    \
      "\"out_txt\":\"" _out_txt "\",\"out_bin\":[" _out_bin "]" \
    "}\n"

    TEST(null,              .input.op_list = {},
                            .output = "");
    TEST(null_cut,          .input.op_list = {OP_CUT},
                            .output = "");
    TEST(null_flushed,      .input.op_list = {OP_FLUSH},
                            .output = "");
    TEST(null_cut_flushed,  .input.op_list = {OP_CUT, OP_FLUSH},
                            .output = "");
    TEST(null_flushed_cut,  .input.op_list = {OP_FLUSH, OP_CUT},
                            .output = "");

    TEST(window_unflushed,
         .input.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200)},
         .output = ""
    );

    TEST(window_cut,
         .input.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_CUT},
         .output = ""
    );

    TEST(window_flushed,
         .input.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_cut_flushed,
         .input.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_CUT, OP_FLUSH},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(window_flushed_cut,
         .input.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_FLUSH, OP_CUT},
         .output = MSG(1, "0", "=100x200", "", "", "", "")
    );

    TEST(min_window,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 0, 0),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=0x0", "", "", "", "")
    );

    TEST(max_window,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, USHRT_MAX, USHRT_MAX),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=65535x65535", "", "", "", "")
    );

    TEST(three_windows,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_WRITE_WINDOW(0, 0, 300, 300),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100=200x200=300x300", "", "", "", "")
    );

    TEST(delay_between_windows,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 100000000, 200, 200),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100+100=200x200", "", "", "", "")
    );

    TEST(window_chunk_overflow,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 10001, 10001),
            OP_WRITE_WINDOW(0, 0, 10002, 10002),
            OP_WRITE_WINDOW(0, 0, 10003, 10003),
            OP_WRITE_WINDOW(0, 0, 10004, 10004),
            OP_WRITE_WINDOW(0, 0, 10005, 10005),
            OP_WRITE_WINDOW(0, 0, 10006, 10006),
            OP_WRITE_WINDOW(0, 0, 10007, 10007),
            OP_WRITE_WINDOW(0, 0, 10008, 10008),
            OP_WRITE_WINDOW(0, 0, 10009, 10009),
            OP_WRITE_WINDOW(0, 0, 10010, 10010),
            OP_FLUSH,
         },
         .output = MSG(1, "0",
                       "=10001x10001=10002x10002=10003x10003"
                       "=10004x10004=10005x10005",
                       "", "", "", "")
                   MSG(2, "0",
                       "=10006x10006=10007x10007=10008x10008"
                       "=10009x10009=10010x10010",
                       "", "", "", "")
    );

    TEST(window_merging,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 1000000, 100, 100),
            OP_WRITE_WINDOW(0, 2000000, 200, 200),
            OP_WRITE_WINDOW(0, 3000000, 200, 200),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100+2=200x200", "", "", "", "")
    );

    TEST(window_not_merging,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 1000000, 200, 200),
            OP_WRITE_WINDOW(0, 2000000, 100, 100),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=100x100+1=200x200+1=100x100", "", "", "", "")
    );

    TEST(window_merging_over_chars,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_WRITE_WINDOW(0, 0, 10, 10),
            OP_WRITE_IO(0, 0, true, "B", 1),
            OP_WRITE_WINDOW(0, 0, 10, 10),
            OP_WRITE_IO(0, 0, true, "C", 1),
            OP_WRITE_WINDOW(0, 0, 20, 20),
            OP_WRITE_IO(0, 0, true, "D", 1),
            OP_WRITE_WINDOW(0, 0, 20, 20),
            OP_WRITE_IO(0, 0, true, "E", 1),
            OP_FLUSH,
         },
         .output = MSG(1, "0", ">1=10x10>2=20x20>2", "", "", "ABCDE", "")
    );

    TEST(window_merging_in_chars,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 10, 10),
            OP_WRITE_IO(0, 1000000, true, "\xf0\x9d", 2),
            OP_WRITE_WINDOW(0, 2000000, 10, 10),
            OP_WRITE_IO(0, 3000000, true, "\x84\x9e", 2),
            OP_FLUSH,
         },
         .output = MSG(1, "0", "=10x10+3>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(char_io_merging,
         .input.op_list = {
            OP_WRITE_IO(0, 1000000, true, "\xf0", 1),
            OP_WRITE_IO(0, 2000000, true, "\x9d", 1),
            OP_WRITE_IO(0, 3000000, true, "\x84", 1),
            OP_WRITE_IO(0, 4000000, true, "\x9e", 1),
            OP_FLUSH,
         },
         .output = MSG(1, "3", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(one_byte,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1)
         },
         .output = ""
    );
    TEST(one_byte_cut,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT
         },
         .output = ""
    );
    TEST(one_byte_flushed,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_flushed_cut,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH,
            OP_CUT
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );
    TEST(one_byte_cut_flushed,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "A", "")
    );

    TEST(input_overflow_flush,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcd", 62),
         },
         .output = MSG(1, "0", "<61",
                       "0123456789abcdef0123456789abcdef"
                       "0123456789abcdef0123456789abc", "",
                       "", "")
    );

    TEST(output_overflow_flush,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcd", 62),
         },
         .output = MSG(1, "0", ">61", "", "",
                       "0123456789abcdef0123456789abcdef"
                       "0123456789abcdef0123456789abc", "")
    );

    TEST(both_overflow_flush,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef", 32),
            OP_WRITE_IO(0, 0, true,
                        "0123456789abcdef0123456789a", 27),
         },
         .output = MSG(1, "0", "<32>26",
                       "0123456789abcdef0123456789abcdef", "",
                       "0123456789abcdef0123456789", "")
    );

    TEST(incomplete,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3)
         },
         .output = ""
    );

    TEST(incomplete_flushed,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_FLUSH
         },
         .output = ""
    );

    TEST(incomplete_cut,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT
         },
         .output = ""
    );

    TEST(incomplete_cut_flushed,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(split_valid,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(split_before_invalid,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84X", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_on_invalid,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_WRITE_IO(0, 0, true, "X", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132")
    );

    TEST(split_incomplete,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84", 1),
            OP_CUT,
            OP_FLUSH
         },
         .output = MSG(1, "0", "]1/3", "", "", "�", "240,157,132")
    );

    TEST(delay_inside_char,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 1100000, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "1", ">1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(delay_between_chars,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+1>1", "A", "", "B", "")
    );

    TEST(delay_between_messages,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, "1", ">1", "", "", "B", "")
    );

    TEST(max_delay_between_windows,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(TLOG_DELAY_MAX_TIMESPEC_SEC,
                            TLOG_DELAY_MAX_TIMESPEC_NSEC,
                            200, 200),
            OP_FLUSH
         },
         .output = MSG(1, "0",
                       "=100x100+" TLOG_DELAY_MAX_MS_STR "=200x200",
                       "", "", "", "")
    );

    TEST(max_delay_inside_char,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, TLOG_DELAY_MAX_MS_STR, ">1",
                       "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(max_delay_between_chars,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1+" TLOG_DELAY_MAX_MS_STR ">1",
                       "A", "", "B", "")
    );

    TEST(max_delay_between_messages,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1", "A", "", "", "") \
                   MSG(2, TLOG_DELAY_MAX_MS_STR, ">1", "", "", "B", "")
    );

    TEST(window_between_chars,
         .input.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "B", 1),
            OP_FLUSH
         },
         .output = MSG(1, "0", "<1=100x200>1", "A", "", "B", "")
    );

    TEST(char_between_windows,
         .input.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x100<1=200x200", "A", "", "", "")
    );

    TEST(window_inside_char,
         .input.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         },
         .output = MSG(1, "0", "=100x200>1", "", "", "\xf0\x9d\x84\x9e", "")
    );

    TEST(windows_chars_and_delays,
         .input.op_list = {
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
