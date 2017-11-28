/*
 * Tlog tlog_json_stream_btoa function test.
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <tltest/misc.h>
#include <tlog/json_misc.h>

#define BOOL_STR(_x) ((_x) ? "true" : "false")

static bool
test(const char *file, int line,
     const char *name, size_t exp_res_len,
     const char *out_ptr, size_t out_len,
     const char *in_ptr, size_t in_len)
{
    bool passed = true;
    size_t res_res_len;
    char res_out_buf[256];
    char exp_out_buf[sizeof(res_out_buf)];
    assert(out_ptr != NULL || out_len == 0);
    assert(out_len <= sizeof(res_out_buf));
    assert(out_len <= sizeof(exp_out_buf));
    assert(in_ptr != NULL || in_len == 0);

    memset(res_out_buf, 0x20, sizeof(res_out_buf));
    memcpy(exp_out_buf, out_ptr, out_len);
    memset(exp_out_buf + out_len, 0x20, sizeof(exp_out_buf) - out_len);

    res_res_len = tlog_json_esc_buf(res_out_buf, out_len, in_ptr, in_len);

#define FAIL(_fmt, _args...) \
    do {                                                            \
        fprintf(stderr, "FAIL %s:%d %s " _fmt "\n", file, line,     \
                name, ##_args);                                     \
        passed = false;                                             \
    } while (0)
#define TEST(_expr, _fmt, _args...) \
    do {                            \
        if (_expr) {                \
            FAIL(_fmt, ##_args);    \
        }                           \
    } while (0)
    TEST(res_res_len != exp_res_len,
         "res_len: %zu != %zu", res_res_len, exp_res_len);
    if (memcmp(res_out_buf, exp_out_buf, sizeof(res_out_buf)) != 0) {
        FAIL("out_buf mismatch:");
        tlog_test_diff(stderr, (uint8_t *)res_out_buf, sizeof(res_out_buf),
                               (uint8_t *)exp_out_buf, sizeof(res_out_buf));
    }
#undef TEST
#undef FAIL
    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, name);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define TEST_BUF_BUF(_name_token, _exp_res_len, \
                     _out_ptr, _out_len, _in_ptr, _in_len)              \
    do {                                                                \
        passed = test(__FILE__, __LINE__, #_name_token, _exp_res_len,   \
                      _out_ptr, _out_len, _in_ptr, _in_len) && passed;  \
    } while (0);

#define TEST_STR_BUF(_name_token, _exp_res_len, \
                     _out_str, _in_ptr, _in_len)    \
    TEST_BUF_BUF(_name_token, _exp_res_len,         \
                 _out_str, strlen(_out_str) + 1,    \
                 _in_ptr, _in_len)

#define TEST_BUF_STR(_name_token, _exp_res_len, \
                     _out_ptr, _out_len, _in_str)               \
    TEST_BUF_BUF(_name_token, _exp_res_len,                     \
                 _out_ptr, _out_len, _in_str, strlen(_in_str))
                 
#define TEST_STR_STR(_name_token, _exp_res_len, \
                     _out_str, _in_str)                     \
    TEST_BUF_STR(_name_token, _exp_res_len,                 \
                 _out_str, strlen(_out_str) + 1, _in_str)

    TEST_STR_STR(empty, 1, "", "");
    TEST_BUF_BUF(zero, 7, "\\u0000", 7, "\0", 1);
    TEST_BUF_BUF(double_zero, 13, "\\u0000\\u0000", 13, "\0\0", 2);
    TEST_STR_STR(one_char_one_byte, 2,
                 "X", "X");
    TEST_STR_STR(one_char_two_bytes, 3,
                 "\xd0\x90", "\xd0\x90");
    TEST_STR_STR(one_char_three_bytes, 4,
                 "\xe5\x96\x9c", "\xe5\x96\x9c");
    TEST_STR_STR(one_char_four_bytes, 5,
                 "\xf0\x9d\x84\x9e", "\xf0\x9d\x84\x9e");
    TEST_STR_STR(two_chars_each_one_byte, 3, "XY", "XY");
    TEST_STR_STR(two_chars_each_two_bytes, 5,
                 "\xd0\x90\xd0\x90", "\xd0\x90\xd0\x90");
    TEST_STR_STR(two_chars_each_three_bytes, 7,
                 "\xe5\x96\x9c\xe5\x96\x9c",
                 "\xe5\x96\x9c\xe5\x96\x9c");
    TEST_STR_STR(two_chars_each_four_bytes, 9,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e",
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");
    TEST_STR_STR(four_chars_all_sizes, 11,
                 "X\xd0\x90\xe5\x96\x9c\xd0\x90\xd0\x90",
                 "X\xd0\x90\xe5\x96\x9c\xd0\x90\xd0\x90");

    TEST_BUF_BUF(zero_space_zero_bytes, 1, "", 0, NULL, 0);
    TEST_BUF_BUF(zero_space_one_byte, 2, "", 0, "X", 1);
    TEST_BUF_BUF(zero_space_two_bytes, 3, "", 0, "XY", 2);

    TEST_BUF_BUF(one_space_zero_bytes, 1, "", 1, NULL, 0);
    TEST_BUF_BUF(one_space_one_byte, 2, "", 1, "X", 1);
    TEST_BUF_BUF(one_space_two_bytes, 3, "", 1, "XY", 2);

    TEST_BUF_BUF(two_spaces_zero_bytes, 1, "\0 ", 2, NULL, 0);
    TEST_BUF_BUF(two_spaces_one_byte, 2, "X", 2, "X", 1);
    TEST_BUF_BUF(two_spaces_two_bytes, 3, "X", 2, "XY", 2);

    TEST_BUF_BUF(three_spaces_zero_bytes, 1, "\0  ", 3, NULL, 0);
    TEST_BUF_BUF(three_spaces_one_byte, 2, "X\0 ", 3, "X", 1);
    TEST_BUF_BUF(three_spaces_two_bytes, 3, "XY", 3, "XY", 2);

    TEST_BUF_STR(cut_two_byte_chars_0, 5,
                 "\xd0\x90", 3, "\xd0\x90\xd0\x90");
    TEST_BUF_STR(cut_two_byte_chars_1, 5,
                 "\xd0\x90\0 ", 4, "\xd0\x90\xd0\x90");
    TEST_BUF_STR(cut_two_byte_chars_2, 5,
                 "\xd0\x90\xd0\x90", 5, "\xd0\x90\xd0\x90");

    TEST_BUF_STR(cut_three_byte_chars_0, 7,
                 "\xe5\x96\x9c", 4,
                 "\xe5\x96\x9c\xe5\x96\x9c");
    TEST_BUF_STR(cut_three_byte_chars_1, 7,
                 "\xe5\x96\x9c\0 ", 5,
                 "\xe5\x96\x9c\xe5\x96\x9c");
    TEST_BUF_STR(cut_three_byte_chars_2, 7,
                 "\xe5\x96\x9c\0  ", 6,
                 "\xe5\x96\x9c\xe5\x96\x9c");
    TEST_BUF_STR(cut_three_byte_chars_3, 7,
                 "\xe5\x96\x9c\xe5\x96\x9c", 7,
                 "\xe5\x96\x9c\xe5\x96\x9c");

    TEST_BUF_STR(cut_four_byte_chars_0, 9,
                 "\xf0\x9d\x84\x9e", 5,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");
    TEST_BUF_STR(cut_four_byte_chars_1, 9,
                 "\xf0\x9d\x84\x9e\0 ", 6,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");
    TEST_BUF_STR(cut_four_byte_chars_2, 9,
                 "\xf0\x9d\x84\x9e\0  ", 7,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");
    TEST_BUF_STR(cut_four_byte_chars_3, 9,
                 "\xf0\x9d\x84\x9e\0   ", 8,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");
    TEST_BUF_STR(cut_four_byte_chars_4, 9,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e", 9,
                 "\xf0\x9d\x84\x9e\xf0\x9d\x84\x9e");

#define TEST_ESC(_char_code_token, _out_str) \
    do {                                                            \
        const char _in_str[] = {_char_code_token};                  \
        TEST_STR_BUF(esc_##_char_code_token, strlen(_out_str) + 1,  \
                     _out_str, _in_str, 1);                         \
    } while (0)
    TEST_ESC(0x00, "\\u0000");
    TEST_ESC(0x01, "\\u0001");
    TEST_ESC(0x02, "\\u0002");
    TEST_ESC(0x03, "\\u0003");
    TEST_ESC(0x04, "\\u0004");
    TEST_ESC(0x05, "\\u0005");
    TEST_ESC(0x06, "\\u0006");
    TEST_ESC(0x07, "\\u0007");
    TEST_ESC(0x08, "\\b");
    TEST_ESC(0x09, "\\t");
    TEST_ESC(0x0a, "\\n");
    TEST_ESC(0x0b, "\\u000b");
    TEST_ESC(0x0c, "\\f");
    TEST_ESC(0x0d, "\\r");
    TEST_ESC(0x0e, "\\u000e");
    TEST_ESC(0x0f, "\\u000f");
    TEST_ESC(0x10, "\\u0010");
    TEST_ESC(0x11, "\\u0011");
    TEST_ESC(0x12, "\\u0012");
    TEST_ESC(0x13, "\\u0013");
    TEST_ESC(0x14, "\\u0014");
    TEST_ESC(0x15, "\\u0015");
    TEST_ESC(0x16, "\\u0016");
    TEST_ESC(0x17, "\\u0017");
    TEST_ESC(0x18, "\\u0018");
    TEST_ESC(0x19, "\\u0019");
    TEST_ESC(0x1a, "\\u001a");
    TEST_ESC(0x1b, "\\u001b");
    TEST_ESC(0x1c, "\\u001c");
    TEST_ESC(0x1d, "\\u001d");
    TEST_ESC(0x1e, "\\u001e");
    TEST_ESC(0x1f, "\\u001f");
    TEST_ESC(0x22, "\\\"");
    TEST_ESC(0x5c, "\\\\");
    TEST_ESC(0x7f, "\\u007f");

    return !passed;
}
