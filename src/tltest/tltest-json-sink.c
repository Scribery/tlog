/*
 * Tlog tlog_json_sink test.
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

#include <tltest/json_sink.h>
#include <tlog/delay.h>
#include <tlog/misc.h>
#include <limits.h>

int
main(void)
{
    bool passed = true;

#define OP_WRITE(_pkt)  TLOG_TEST_JSON_SINK_OP_WRITE(_pkt)
#define OP_FLUSH        TLOG_TEST_JSON_SINK_OP_FLUSH
#define OP_CUT          TLOG_TEST_JSON_SINK_OP_CUT

#define OP_WRITE_WINDOW(_pkt_window_args...) \
    OP_WRITE(TLOG_PKT_WINDOW(_pkt_window_args))

#define OP_WRITE_IO(_pkt_io_args...) \
    OP_WRITE(TLOG_PKT_IO(_pkt_io_args))

#define MSG(_id_tkn, _pos, _timing, \
            _in_txt, _in_bin, _out_txt, _out_bin)                   \
    "{\"ver\":\"2.2\",\"host\":\"localhost\",\"rec\":\"rec-1\","    \
      "\"user\":\"user\",\"term\":\"xterm\",\"session\":1,"         \
      "\"id\":" #_id_tkn ",\"pos\":" _pos ","                       \
      "\"timing\":\"" _timing "\","                                 \
      "\"in_txt\":\"" _in_txt "\",\"in_bin\":[" _in_bin "],"        \
      "\"out_txt\":\"" _out_txt "\",\"out_bin\":[" _out_bin "]"     \
    "}\n"

#define INPUT(_struct_init_args...) \
    .input = {                      \
        .chunk_size = 64,           \
        .hostname = "localhost",    \
        .recording = "rec-1",       \
        .username = "user",         \
        .terminal = "xterm",        \
        .session_id = 1,            \
        _struct_init_args           \
    }

#define OUTPUT(_string) \
    .output = (_string)

#define TEST(_name_token, _struct_init_args...) \
    passed = tlog_test_json_sink(               \
                __FILE__, __LINE__,             \
                #_name_token,                   \
                (struct tlog_test_json_sink){   \
                    _struct_init_args           \
                }                               \
    ) && passed

    TEST(null,              INPUT(.op_list = {}),
                            OUTPUT(""));
    TEST(null_cut,          INPUT(.op_list = {OP_CUT}),
                            OUTPUT(""));
    TEST(null_flushed,      INPUT(.op_list = {OP_FLUSH}),
                            OUTPUT(""));
    TEST(null_cut_flushed,  INPUT(.op_list = {OP_CUT, OP_FLUSH}),
                            OUTPUT(""));
    TEST(null_flushed_cut,  INPUT(.op_list = {OP_FLUSH, OP_CUT}),
                            OUTPUT(""));

    TEST(window_unflushed,
         INPUT(.op_list = {
             OP_WRITE_WINDOW(0, 0, 100, 200)
         }),
         OUTPUT("")
    );

    TEST(window_cut,
         INPUT(.op_list = {
             OP_WRITE_WINDOW(0, 0, 100, 200),
             OP_CUT
         }),
         OUTPUT("")
    );

    TEST(window_flushed,
         INPUT(.op_list = {OP_WRITE_WINDOW(0, 0, 100, 200), OP_FLUSH}),
         OUTPUT(MSG(1, "0", "=100x200", "", "", "", ""))
    );

    TEST(window_cut_flushed,
         INPUT(.op_list = {
             OP_WRITE_WINDOW(0, 0, 100, 200),
             OP_CUT,
             OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200", "", "", "", ""))
    );

    TEST(window_flushed_cut,
         INPUT(.op_list = {
             OP_WRITE_WINDOW(0, 0, 100, 200),
             OP_FLUSH,
             OP_CUT
         }),
         OUTPUT(MSG(1, "0", "=100x200", "", "", "", ""))
    );

    TEST(min_window,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 0, 0),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=0x0", "", "", "", ""))
    );

    TEST(max_window,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, USHRT_MAX, USHRT_MAX),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=65535x65535", "", "", "", ""))
    );

    TEST(three_windows,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_WRITE_WINDOW(0, 0, 300, 300),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=100x100=200x200=300x300", "", "", "", ""))
    );

    TEST(delay_between_windows,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 100000000, 200, 200),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=100x100+100=200x200", "", "", "", ""))
    );

    TEST(window_chunk_overflow,
         INPUT(.op_list = {
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
         }),
         OUTPUT(MSG(1, "0",
                    "=10001x10001=10002x10002=10003x10003"
                    "=10004x10004=10005x10005",
                    "", "", "", "")
                MSG(2, "0",
                    "=10006x10006=10007x10007=10008x10008"
                    "=10009x10009=10010x10010",
                    "", "", "", ""))
    );

    TEST(window_merging,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 1000000, 100, 100),
            OP_WRITE_WINDOW(0, 2000000, 200, 200),
            OP_WRITE_WINDOW(0, 3000000, 200, 200),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=100x100+2=200x200", "", "", "", ""))
    );

    TEST(window_not_merging,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 1000000, 200, 200),
            OP_WRITE_WINDOW(0, 2000000, 100, 100),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=100x100+1=200x200+1=100x100", "", "", "", ""))
    );

    TEST(window_merging_over_chars,
         INPUT(.op_list = {
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
         }),
         OUTPUT(MSG(1, "0", ">1=10x10>2=20x20>2", "", "", "ABCDE", ""))
    );

    TEST(window_merging_in_chars,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 10, 10),
            OP_WRITE_IO(0, 1000000, true, "\xf0\x9d", 2),
            OP_WRITE_WINDOW(0, 2000000, 10, 10),
            OP_WRITE_IO(0, 3000000, true, "\x84\x9e", 2),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", "=10x10+3>1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(char_io_merging,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 1000000, true, "\xf0", 1),
            OP_WRITE_IO(0, 2000000, true, "\x9d", 1),
            OP_WRITE_IO(0, 3000000, true, "\x84", 1),
            OP_WRITE_IO(0, 4000000, true, "\x9e", 1),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "3", ">1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(one_byte,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1)
         }),
         OUTPUT("")
    );
    TEST(one_byte_cut,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT
         }),
         OUTPUT("")
    );
    TEST(one_byte_flushed,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", ">1", "", "", "A", ""))
    );
    TEST(one_byte_flushed_cut,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_FLUSH,
            OP_CUT
         }),
         OUTPUT(MSG(1, "0", ">1", "", "", "A", ""))
    );
    TEST(one_byte_cut_flushed,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "A", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", ">1", "", "", "A", ""))
    );

    TEST(input_overflow_flush,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcd", 62),
         }),
         OUTPUT(MSG(1, "0", "<61",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abc", "",
                    "", ""))
    );

    TEST(output_overflow_flush,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcd", 62),
         }),
         OUTPUT(MSG(1, "0", ">61", "", "",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abc", ""))
    );

    TEST(both_overflow_flush,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef", 32),
            OP_WRITE_IO(0, 0, true,
                        "0123456789abcdef0123456789a", 27),
         }),
         OUTPUT(MSG(1, "0", "<32>26",
                    "0123456789abcdef0123456789abcdef", "",
                    "0123456789abcdef0123456789", ""))
    );

    TEST(no_window_no_window_repeat,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abc"
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abc", (64 - 3) * 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<61",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abc",
                    "", "", "")
                MSG(2, "0", "<61",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abc",
                    "", "", ""))
    );

    TEST(no_write_no_window_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234"
                        "\xf0\x9d",
                        32 + 16 + 5 + 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234"
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        (32 + 16 + 5) * 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_repeat_after_delay,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        (32 + 16 + 5)),
            OP_WRITE_IO(1, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        (32 + 16 + 5)),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "1000", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_repeat_non_initial,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "0123456789abcdef0123456789abcdef", 32),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef01"
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        (16 + 2 + 32 + 16 + 5)),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<32=100x200<18",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01",
                    "", "", "")
                MSG(2, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_change_no_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_WRITE_WINDOW(0, 0, 300, 400),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=300x400<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_delayed_change,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_WRITE_WINDOW(0, 1000000, 300, 400),
            OP_WRITE_IO(0, 1000000, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "1", "=300x400<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_same_no_double_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_double_same_no_double_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef01234",
                        32 + 16 + 5),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", ""))
    );

    TEST(window_change_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcdef",
                        64),
            OP_WRITE_WINDOW(0, 0, 300, 400),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcde",
                        31),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=100x200<11=300x400<31",
                    "56789abcdef0123456789abcdef0123456789abcde",
                    "", "", ""))
    );

    TEST(window_delayed_change_repeat,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789abcdef",
                        64),
            OP_WRITE_WINDOW(0, 1000000, 300, 400),
            OP_WRITE_IO(0, 1000000, false,
                        "0123456789abcdef0123456789abc",
                        29),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200<53",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef01234",
                    "", "", "")
                MSG(2, "0", "=100x200<11+1=300x400<29",
                    "56789abcdef0123456789abcdef0123456789abc",
                    "", "", ""))
    );

    TEST(incomplete,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3)
         }),
         OUTPUT("")
    );

    TEST(invalid_byte,
         INPUT(.op_list = {
            OP_WRITE_IO(1, 0, true, "\xff", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "]1/1",
                    "", "", "\xef\xbf\xbd", "255"))
    );

    TEST(invalid_byte_after_window,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(1, 0, 100, 100),
            OP_WRITE_IO(2, 0, true, "\xff", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x100+1000]1/1",
                    "", "", "\xef\xbf\xbd", "255"))
    );

    TEST(late_character_termination,
         INPUT(.op_list = {
            OP_WRITE_IO(1, 0, false, "\xf0\x9d", 2),
            OP_WRITE_IO(2, 0, true, "x", 1),
            OP_WRITE_IO(3, 0, false, "\xff", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "1000", "[1/2>1+1000[1/1",
                    "\xef\xbf\xbd\xef\xbf\xbd", "240,157,255",
                    "x", ""))
    );

    TEST(late_character_cut,
         INPUT(.op_list = {
            OP_WRITE_IO(1, 0, false, "\xf0\x9d", 2),
            OP_WRITE_IO(2, 0, true, "x", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "1000", "[1/2>1",
                    "\xef\xbf\xbd", "240,157",
                    "x", ""))
    );

    TEST(incomplete_flushed,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_FLUSH
         }),
         OUTPUT("")
    );

    TEST(incomplete_cut,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT
         }),
         OUTPUT("")
    );

    TEST(incomplete_cut_flushed,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "]1/3", "", "", "�", "240,157,132"))
    );

    TEST(split_valid,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", ">1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(split_before_invalid,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84X", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132"))
    );

    TEST(split_on_invalid,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d\x84", 3),
            OP_WRITE_IO(0, 0, true, "X", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "]1/3>1", "", "", "�X", "240,157,132"))
    );

    TEST(split_incomplete,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 0, true, "\x84", 1),
            OP_CUT,
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "]1/3", "", "", "�", "240,157,132"))
    );

    TEST(delay_inside_char,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 1100000, true, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "1", ">1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(delay_between_chars,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1+1>1", "A", "", "B", ""))
    );

    TEST(delay_between_messages,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(0, 1100000, true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1", "A", "", "", "") \
                MSG(2, "1", ">1", "", "", "B", ""))
    );

    TEST(min_delay_between_windows,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(0, 1000000, 200, 200),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x100+1=200x200", "", "", "", ""))
    );

    TEST(max_delay_between_windows,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_WINDOW(TLOG_DELAY_MAX_TIMESPEC_SEC,
                            TLOG_DELAY_MAX_TIMESPEC_NSEC,
                            200, 200),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0",
                    "=100x100+" TLOG_DELAY_MAX_MS_STR "=200x200",
                    "", "", "", ""))
    );

    TEST(min_delay_inside_char,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(0, 1000000, true, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "1", ">1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(max_delay_inside_char,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, TLOG_DELAY_MAX_MS_STR, ">1",
                    "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(min_delay_between_chars,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(0, 1000000, true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1+1>1", "A", "", "B", ""))
    );

    TEST(max_delay_between_chars,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1+" TLOG_DELAY_MAX_MS_STR ">1",
                    "A", "", "B", ""))
    );

    TEST(max_delay_between_messages,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_FLUSH,
            OP_WRITE_IO(TLOG_DELAY_MAX_TIMESPEC_SEC,
                        TLOG_DELAY_MAX_TIMESPEC_NSEC,
                        true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1", "A", "", "", "") \
                MSG(2, TLOG_DELAY_MAX_MS_STR, ">1", "", "", "B", ""))
    );

    TEST(window_between_chars,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "B", 1),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<1=100x200>1", "A", "", "B", ""))
    );

    TEST(char_between_windows,
         INPUT(.op_list = {
            OP_WRITE_WINDOW(0, 0, 100, 100),
            OP_WRITE_IO(0, 0, false, "A", 1),
            OP_WRITE_WINDOW(0, 0, 200, 200),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x100<1=200x200", "A", "", "", ""))
    );

    TEST(window_inside_char,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, true, "\xf0\x9d", 2),
            OP_WRITE_WINDOW(0, 0, 100, 200),
            OP_WRITE_IO(0, 0, true, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "=100x200>1", "", "", "\xf0\x9d\x84\x9e", ""))
    );

    TEST(windows_chars_and_delays,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 10000000, true, "S", 1),
            OP_WRITE_WINDOW(0, 30000000, 100, 100),
            OP_WRITE_WINDOW(0, 60000000, 200, 200),
            OP_WRITE_IO(0, 100000000, true, "E", 1),
            OP_FLUSH,
         }),
         OUTPUT(MSG(1, "0", ">1+20=100x100+30=200x200+40>1",
                    "", "", "SE", ""))
    );

    TEST(incomplete_char_backoff,
         INPUT(.op_list = {
            OP_WRITE_IO(0, 0, false,
                        "0123456789abcdef0123456789abcdef"
                        "0123456789abcdef0123456789a\xf0\x9d", 61),
            OP_WRITE_IO(0, 0, false, "\x84\x9e", 2),
            OP_FLUSH
         }),
         OUTPUT(MSG(1, "0", "<59",
                    "0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789a", "",
                    "", "")
                MSG(2, "0", "<1",
                    "\xf0\x9d\x84\x9e", "",
                    "", ""))
    );

    return !passed;
}
