/*
 * Tlog tlog_json_source test.
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

#include <string.h>
#include <tltest/json_source.h>
#include <tlog/rc.h>
#include <tlog/grc.h>
#include <tlog/delay.h>
#include <tlog/misc.h>
#include <json_tokener.h>

#define TLTEST_MSG_MAX_SIZE 3072
#define TLTEST_MSG_MAX_COMBINED_SIZE 3072*3

void
tltest_json_source_fmt_msg(size_t id,
                  const char *pos,
				  const char *timing,
				  const char *in_txt,
				  const char *in_bin,
				  const char *out_txt,
				  const char *out_bin,
                  bool latest_version,
                  char *_msg)

{
    char msg[TLTEST_MSG_MAX_SIZE];

    /* https://github.com/Scribery/tlog/blob/master/doc/log_format.md
     * version 2.3 */
    if (latest_version) {
        snprintf(
            msg, TLTEST_MSG_MAX_SIZE,
            "{"
                "\"ver\":"      "\"1\","
                "\"host\":"     "\"%s\","
                "\"rec\":"      "\"%s\","
                "\"user\":"     "\"%s\","
                "\"term\":"     "\"%s\","
                "\"session\":"  "%u,"
                "\"id\":"       "%zu,"
                "\"pos\":"      "%s,"
                "\"time\":"     "%f,"
                "\"timing\":"   "\"%s\","
                "\"in_txt\":"   "\"%s\","
                "\"in_bin\":"   "[%s],"
                "\"out_txt\":"  "\"%s\","
                "\"out_bin\":"  "[%s]"
            "}\n",
            "host", "5d24f15", "user", "xterm", 1,
            id, pos, 1600710269.999, timing, in_txt, in_bin,
            out_txt, out_bin);
    /* version 2.2 */
    } else {
        snprintf(
            msg, TLTEST_MSG_MAX_SIZE,
            "{"
                "\"ver\":"      "\"1\","
                "\"host\":"     "\"%s\","
                "\"user\":"     "\"%s\","
                "\"term\":"     "\"%s\","
                "\"session\":"  "%u,"
                "\"id\":"       "%zu,"
                "\"pos\":"      "%s,"
                "\"timing\":"   "\"%s\","
                "\"in_txt\":"   "\"%s\","
                "\"in_bin\":"   "[%s],"
                "\"out_txt\":"  "\"%s\","
                "\"out_bin\":"  "[%s]"
            "}\n",
            "host", "user", "xterm", 1,
            id, pos, timing, in_txt, in_bin,
            out_txt, out_bin);
    }

    strcpy(_msg, msg);
}

void
tltest_json_source_cat_msg(const char *msg1,
                  const char *msg2,
                  const char *msg3,
                  char *_msg)
{

    char msg[TLTEST_MSG_MAX_COMBINED_SIZE];
    strcpy(msg, msg1);
    strcat(msg, msg2);
    strcat(msg, msg3);

    strcpy(_msg, msg);
}

int
main(void)
{
    bool passed = true;
    bool curr_version = false;

#define PKT_VOID \
    TLOG_PKT_VOID
#define PKT_WINDOW(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
                   _width, _height) \
    TLOG_PKT_WINDOW(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
                    _width, _height)
#define PKT_IO(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
               _output, _buf, _len) \
    TLOG_PKT_IO(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
                _output, _buf, _len)
#define PKT_IO_STR(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
                    _output, _buf) \
    TLOG_PKT_IO_STR(_tv_sec, _tv_nsec, _real_sec, _real_nsec, \
                    _output, _buf)

#define OP_LOC_GET(_exp_loc) \
    TLTEST_JSON_SOURCE_OP_LOC_GET(_exp_loc)
#define OP_READ(_exp_grc, _exp_pkt) \
    TLTEST_JSON_SOURCE_OP_READ(_exp_grc, _exp_pkt)

#define MSG_SPEC(_host_token, _user_token, _term_token, _session_token, \
                 _id_token, _pos,                                       \
                 _timing, _in_txt, _in_bin, _out_txt, _out_bin)         \
    "{"                                                                 \
        "\"ver\":"      "1,"                                            \
        "\"host\":"     "\"" #_host_token "\","                         \
        "\"user\":"     "\"" #_user_token "\","                         \
        "\"term\":"     "\"" #_term_token "\","                         \
        "\"session\":"  #_session_token ","                             \
        "\"id\":"       #_id_token ","                                  \
        "\"pos\":"      _pos ","                                        \
        "\"timing\":"   "\"" _timing "\","                              \
        "\"in_txt\":"   "\"" _in_txt "\","                              \
        "\"in_bin\":"   "[" _in_bin "],"                                \
        "\"out_txt\":"  "\"" _out_txt "\","                             \
        "\"out_bin\":"  "[" _out_bin "]"                                \
    "}\n"

#define MSG_DUMMY(_id_token, _pos, \
                  _timing, _in_txt, _in_bin, _out_txt, _out_bin)    \
    MSG_SPEC(host, user, xterm, 1, _id_token, _pos,                 \
                 _timing, _in_txt, _in_bin, _out_txt, _out_bin)

#define OP_READ_OK(_exp_pkt) \
    OP_READ(TLOG_RC_OK, _exp_pkt)

#define INPUT(_string) \
    .input = (_string)

#define OUTPUT(_struct_init_args...) \
    .output = {                         \
        .hostname = NULL,               \
        .username = NULL,               \
        .terminal = NULL,               \
        .session_id = 0,                \
        _struct_init_args               \
    }

#define TEST(_name_token, _struct_init_args...) \
    passed = tltest_json_source(                                \
                __FILE__, __LINE__,                             \
                #_name_token,                                   \
                (struct tltest_json_source){_struct_init_args}  \
             ) && passed

    char msg[TLTEST_MSG_MAX_SIZE];
    char msg2[TLTEST_MSG_MAX_SIZE];
    char msg3[TLTEST_MSG_MAX_SIZE];
    char combined_msg[TLTEST_MSG_MAX_COMBINED_SIZE];


    /* Tests are all executed twice, once with the latest message format,
     * and again with the previous format to ensure we remain backwards
     * compatible when the message format changes */
    do {
        curr_version = !curr_version;

        TEST(null,
             INPUT(""),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                }
             )
        );

        TEST(null_eof,
             INPUT(""),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        TEST(null_repeat_eof,
             INPUT(""),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200", "", "", "", "", curr_version, msg);
        TEST(window,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200", "", "", "", "", curr_version, msg);
        TEST(window_eof,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120=210x220", "", "", "", "", curr_version, msg);
        TEST(two_windows,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 210, 220))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200=300x400", "", "", "", "", curr_version, msg);
        TEST(two_windows_eof,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 300, 400)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        TEST(loc_null,
             INPUT(""),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_LOC_GET(1)
                }
             )
        );

        TEST(loc_null_after_eof,
             INPUT(""),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID),
                    OP_LOC_GET(1)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200", "", "", "", "", curr_version, msg);
        TEST(loc_after_window,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_LOC_GET(2)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200=300x400", "", "", "", "", curr_version, msg);
        TEST(loc_after_two_windows,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 300, 400)),
                    OP_LOC_GET(2)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "0", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_0,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 0, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_1ms,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 1000000, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "999", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_999ms,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 999000000, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_1s,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, TLOG_DELAY_MAX_MS_STR,
                          "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_max,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_OK,
                            PKT_WINDOW(TLOG_DELAY_MAX_TIMESPEC_SEC,
                                       TLOG_DELAY_MAX_TIMESPEC_NSEC,
                                       0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1" TLOG_DELAY_MAX_MS_STR,
                          "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_overflow,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_POS, PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "-1", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_underflow,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_POS, PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "0", "=100x200", "", "", "", "", curr_version, msg);
        TEST(pos_min,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_OK, PKT_WINDOW(0, 0, 0, 0, 100, 200))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "id\":\"bbb", "", "", "", "", "", curr_version, msg);
        TEST(syntax_error,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                     PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "id\":\"bbb", "", "", "", "", "", curr_version, msg);
        TEST(loc_after_syntax_error,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                     PKT_VOID),
                    OP_LOC_GET(2)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(1, "id\":\"bbb", "", "", "", "", "", curr_version, msg2);
        tltest_json_source_fmt_msg(2, "2000", "=210x220", "", "", "", "", curr_version, msg3);
        tltest_json_source_cat_msg(msg, msg2, msg3, combined_msg);

        TEST(syntax_error_recovery,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_GRC_FROM(json, json_tokener_error_parse_unexpected),
                     PKT_VOID),
                    OP_READ_OK(PKT_WINDOW(2, 0, 0, 0, 210, 220))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "2000", "=210x220", "", "", "", "", curr_version, msg3);
        tltest_json_source_cat_msg(msg, "{\"abc\": \"def\"}\n", msg3, combined_msg);

        TEST(schema_error_recovery,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_MISSING, PKT_VOID),
                    OP_READ_OK(PKT_WINDOW(2, 0, 0, 0, 210, 220))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "2000", "=210x220", "", "", "", "", curr_version, msg3);
        /* First field generates string "0" id */
        tltest_json_source_cat_msg(msg, MSG_DUMMY("0", "0", "", "", "", "", ""), msg3, combined_msg);

        TEST(type_error_recovery,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_TYPE, PKT_VOID),
                    OP_READ_OK(PKT_WINDOW(2, 0, 0, 0, 210, 220))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "2000", "=655536x0", "", "", "", "", curr_version, msg2);
        tltest_json_source_fmt_msg(3, "3000", "=210x220", "", "", "", "", curr_version, msg3);
        tltest_json_source_cat_msg(msg, msg2, msg3, combined_msg);

        TEST(value_error_recovery,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TIMING,
                            PKT_VOID),
                    OP_READ_OK(PKT_WINDOW(3, 0, 0, 0, 210, 220))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=100x200", "", "", "", "", curr_version, msg);
        TEST(window_repeat_eof,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "", "", "", "", "", curr_version, msg);
        TEST(io_null,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "X", "", "", "", curr_version, msg);
        TEST(io_in_1_txt1,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", ">1", "", "", "X", "", curr_version, msg);
        TEST(io_out_1_txt1,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "X")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1>1<1>1", "AC", "", "BD", "", curr_version, msg);
        TEST(io_mixed_txt,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "A")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "B")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "C")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "D")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1", " <1 +1 >1 +1 <1 +1 >1 ",
                          "AC", "", "BD", "", curr_version, msg);
        TEST(io_spaced_timing,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(0, 1000000, 0, 0, false, "A")),
                    OP_READ_OK(PKT_IO_STR(0, 2000000, 0, 0, true, "B")),
                    OP_READ_OK(PKT_IO_STR(0, 3000000, 0, 0, false, "C")),
                    OP_READ_OK(PKT_IO_STR(0, 4000000, 0, 0, true, "D")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1]0/1[0/1]0/1",
                          "", "65,67", "", "66,68", curr_version, msg);
        TEST(io_mixed_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "A")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "B")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "C")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "D")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<2", "XY", "", "", "", curr_version, msg);
        TEST(io_in_2_txt1,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "XY")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", ">2", "", "", "XY", "", curr_version, msg);
        TEST(io_out_2_txt1,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "XY")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "\xd0\x90", "", "", "", curr_version, msg);
        TEST(io_in_1_txt2,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xd0\x90")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "\xe5\x96\x9c", "", "", "", curr_version, msg);
        TEST(io_in_1_txt3,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xe5\x96\x9c")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "\xf0\x9d\x84\x9e", "", "", "", curr_version, msg);
        TEST(io_in_1_txt4,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x84\x9e")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<3",
                          "\xf0\x9d\x85\x9d"
                          "\xf0\x9d\x85\x9e"
                          "\xf0\x9d\x85\x9f",
                          "", "", "", curr_version, msg);
        TEST(io_in_txt_split,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9d")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9e")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9f")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<7",
                          "12345"
                          "\xf0\x9d\x85\x9d"
                          "\xf0\x9d\x85\x9e",
                          "", "", "", curr_version, msg);
       TEST(io_in_txt_split_uneven,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "1234")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "5")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9d")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9e")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1<2<3", "ABCXYZ", "", "", "", curr_version, msg);
        TEST(io_in_merge_txt,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "ABCXYZ")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1[0/2[0/3",
                             "", "49,50,51,52,53,54", "", "", curr_version, msg);
        TEST(io_in_merge_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "123456")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1[0/1<1[0/2<1",
                             "136", "50,52,53", "", "", curr_version, msg);
        TEST(io_in_merge_mixed,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "123456")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+0<1", "AB", "", "", "", curr_version, msg);
        TEST(io_in_merge_zero_delay,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "AB")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1>0<1", "AB", "", "", "", curr_version, msg);
        TEST(io_in_merge_zero_out,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "AB")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1>1<1", "AC", "", "B", "", curr_version, msg);
        TEST(io_in_merge_break_out,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "A")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "B")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "C")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+1000<1", "AB", "", "", "", curr_version, msg);
        TEST(io_in_merge_break_delay,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "A")),
                    OP_READ_OK(PKT_IO_STR(2, 0, 0, 0, false, "B")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1<1", "Y", "88", "", "", curr_version, msg);
        TEST(io_in_0_txt_1_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "XY")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/2<1", "Z", "88, 89", "", "", curr_version, msg);
        TEST(io_in_0_txt_2_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "XYZ")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[1/1", "X", "89", "", "", curr_version, msg);
        TEST(io_in_1_txt1_1_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[1/1", "X", "89", "", "", curr_version, msg);
        TEST(io_in_1_txt1_1_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[2/1", "XY", "90", "", "", curr_version, msg);
        TEST(io_in_2_txt1_1_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "Z")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<4+1000<4", "12345678", "", "", "", curr_version, msg);
        TEST(io_in_delay_on_boundary,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "1234")),
                    OP_READ_OK(PKT_IO_STR(2, 0, 0, 0, false, "5678")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+1000<2",
                        "X\xf0\x9d\x85\x9dY", "", "", "", curr_version, msg);
        TEST(io_in_delay_before_uneven_boundary,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(2, 0, 0, 0, false, "\xf0\x9d\x85\x9d")),
                    OP_READ_OK(PKT_IO_STR(2, 0, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<2+1000<1",
                          "X\xf0\x9d\x85\x9dY", "", "", "", curr_version, msg);
        TEST(io_in_delay_after_uneven_boundary,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "\xf0\x9d\x85\x9d")),
                    OP_READ_OK(PKT_IO_STR(2, 0, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "\xf0\x9d\x85", "", "", "", curr_version, msg);
        TEST(io_in_short_char,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/2", "", "48", "", "", curr_version, msg);
        TEST(io_in_short_bin,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1", "", "3.14", "", "", curr_version, msg);
        TEST(io_in_bin_not_int,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/0", "", "", "", "", curr_version, msg);
        TEST(io_in_bin_void,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "", "", "", "", curr_version, msg);
        TEST(io_in_txt_too_short,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_TXT, PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "", "X", "", "", "", curr_version, msg);
        TEST(io_in_txt_too_long_0,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1", "XY", "", "", "", curr_version, msg);
        TEST(io_in_txt_too_long_1,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X"))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1", "", "0", "", "", curr_version, msg);
        TEST(io_in_bin_min,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO(1, 0, 0, 0, false, "\0", 1)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1", "", "255", "", "", curr_version, msg);
        TEST(io_in_bin_max,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO(1, 0, 0, 0, false, "\xff", 1)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1", "", "-1", "", "", curr_version, msg);
        TEST(io_in_bin_too_small,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "[0/1", "", "256", "", "", curr_version, msg);
        TEST(io_in_bin_too_big,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ(TLOG_RC_JSON_MSG_FIELD_INVALID_VALUE_BIN, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<3", "X\\u0000Y", "", "", "", curr_version, msg);
        TEST(io_in_txt_zero_char,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO(1, 0, 0, 0, false, "X\0Y", 3))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1[1/1+234>1]1/1",
                          "1.", "50", "3.", "52", curr_version, msg);
        TEST(io_everything,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "12")),
                    OP_READ_OK(PKT_IO_STR(1, 234000000, 0, 0, true, "34"))
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(1, "2000", "=210x220", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(id_repeat,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(0, "2000", "=210x220", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(id_backwards,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(3, "2000", "=210x220", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(id_gap,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_SOURCE_MSG_ID_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1000", "=210x220", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(ts_repeat,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 210, 220)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "=110x120", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "500", "=210x220", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(ts_backwards,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 110, 120)),
                    OP_READ(TLOG_RC_JSON_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1000", "<1", "X", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(io_ts_null_no_overlap,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+1", "X", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1000", "<1", "Y", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(io_ts_hanging_no_overlap,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+1<1", "XY", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1000", "<1", "Z", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(io_ts_repeat,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(1, 1000000, 0, 0, false, "Y")),
                    OP_READ(TLOG_RC_JSON_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+2<1", "XY", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1001", "<1", "Z", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(io_ts_overlap,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(1, 2000000, 0, 0, false, "Y")),
                    OP_READ(TLOG_RC_JSON_SOURCE_PKT_TS_OUT_OF_ORDER, PKT_VOID),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1+2<1", "XY", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "1002", "<1", "Z", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(io_ts_no_gap,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "X")),
                    OP_READ_OK(PKT_IO_STR(1, 2000000, 0, 0, false, "Y")),
                    OP_READ_OK(PKT_IO_STR(1, 2000000, 0, 0, false, "Z")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "+1000=110x120+1000=210x220",
                          "", "", "", "", curr_version, msg);
        TEST(delays_and_windows,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(2, 0, 0, 0, 110, 120)),
                    OP_READ_OK(PKT_WINDOW(3, 0, 0, 0, 210, 220)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<1=100x200<1", "AB", "", "", "", curr_version, msg);
        TEST(window_between_chars,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "A")),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "B")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<3=100x200<3",
                          "ABCXYZ", "", "", "", curr_version, msg);
        TEST(window_between_strings,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "ABC")),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "XYZ")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "1000", "<3=100x200>3", "ABC", "", "XYZ", "", curr_version, msg);
        TEST(window_between_streams,
             INPUT(msg),
             OUTPUT(
                .io_size = 6,
                .op_list = {
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, false, "ABC")),
                    OP_READ_OK(PKT_WINDOW(1, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_IO_STR(1, 0, 0, 0, true, "XYZ")),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "0", "=100x200+100=100x200",
                             "", "", "", "", curr_version, msg);
        TEST(window_suppression_in_message,
             INPUT(msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "0", "=100x200", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "0", "=100x200", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(window_suppression_between_messages,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

        tltest_json_source_fmt_msg(1, "0", "=100x200", "", "", "", "", curr_version, msg);
        tltest_json_source_fmt_msg(2, "100", "=100x200", "", "", "", "", curr_version, msg2);
        tltest_json_source_cat_msg(msg, msg2, "", combined_msg);
        TEST(window_suppression_between_messages_with_delay,
             INPUT(combined_msg),
             OUTPUT(
                .io_size = 4,
                .op_list = {
                    OP_READ_OK(PKT_WINDOW(0, 0, 0, 0, 100, 200)),
                    OP_READ_OK(PKT_VOID)
                }
             )
        );

    } while (curr_version);

    return !passed;
}
