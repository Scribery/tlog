/*
 * Tlog tlog_stream test.
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
#include <string.h>
#include <tlog/stream.h>
#include <tlog/test_misc.h>

#define SIZE    TLOG_STREAM_SIZE_MIN

enum op_type {
    OP_TYPE_NONE,
    OP_TYPE_WRITE,
    OP_TYPE_FLUSH,
    OP_TYPE_CUT,
    OP_TYPE_EMPTY,
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
    case OP_TYPE_EMPTY:
        return "empty";
    default:
        return "<unknown>";
    }
}

struct op_data_write {
    uint8_t     buf[SIZE];
    size_t      len_in;
    size_t      len_out;
    ssize_t     meta_off;
    ssize_t     rem_off;
};

struct op_data_flush {
    ssize_t     meta_off;
};

struct op_data_cut {
    ssize_t     meta_off;
    ssize_t     rem_off;
};

struct op {
    enum op_type type;
    union {
        struct op_data_write    write;
        struct op_data_flush    flush;
        struct op_data_cut      cut;
    } data;
};

struct test {
    struct op       op_list[16];
    size_t          rem_in;
    size_t          rem_out;
    const uint8_t   txt_buf[SIZE];
    size_t          txt_len;
    const uint8_t   bin_buf[SIZE];
    size_t          bin_len;
    const uint8_t   meta_buf[SIZE];
    size_t          meta_len;
};

static bool
test(const char *n, const struct test t)
{
    bool passed = true;
    tlog_grc grc;
    uint8_t meta_buf[SIZE] = {0,};
    struct tlog_stream stream;
    const uint8_t *buf;
    size_t len;
    uint8_t *meta_last = meta_buf;
    uint8_t *meta_next = meta_buf;
    size_t rem_last;
    size_t rem_next;
    const struct op *op;

    assert(t.rem_in <= SIZE);

    rem_next = rem_last = t.rem_in;

    grc = tlog_stream_init(&stream, SIZE, '<', '[');
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed initializing the stream: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }

    memset(stream.txt_buf, 0, stream.size);
    memset(stream.bin_buf, 0, stream.size);

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
        case OP_TYPE_WRITE:
            assert(op->data.write.len_out <= op->data.write.len_in);
            buf = op->data.write.buf;
            len = op->data.write.len_in;
            tlog_stream_write(&stream, &buf, &len, &meta_next, &rem_next);
            if ((buf < op->data.write.buf) ||
                (buf - op->data.write.buf) !=
                    (ssize_t)(op->data.write.len_in -
                              op->data.write.len_out))
                FAIL_OP("off %zd != %zu",
                        (buf - op->data.write.buf),
                        (op->data.write.len_in - op->data.write.len_out));
            if (len != op->data.write.len_out)
                FAIL_OP("len %zu != %zu", len, op->data.write.len_out);
            if ((meta_next - meta_last) != op->data.write.meta_off)
                FAIL_OP("meta_off %zd != %zd",
                        (meta_next - meta_last), op->data.write.meta_off);
            meta_last = meta_next;
            if (((ssize_t)rem_last - (ssize_t)rem_next) !=
                    op->data.write.rem_off)
                FAIL_OP("rem_off %zd != %zd",
                        (rem_last - rem_next), op->data.write.rem_off);
            rem_last = rem_next;
            if (passed)
                break;
            else
                goto cleanup;
        case OP_TYPE_FLUSH:
            tlog_stream_flush(&stream, &meta_next);
            if ((meta_next - meta_last) != op->data.flush.meta_off)
                FAIL_OP("meta_off %zd != %zd",
                        (meta_next - meta_last),
                        op->data.flush.meta_off);
            meta_last = meta_next;
            if (passed)
                break;
            else
                goto cleanup;
        case OP_TYPE_CUT:
            tlog_stream_cut(&stream, &meta_next, &rem_next);
            if ((meta_next - meta_last) != op->data.cut.meta_off)
                FAIL_OP("meta_off %zd != %zd",
                        (meta_next - meta_last), op->data.cut.meta_off);
            meta_last = meta_next;
            if (((ssize_t)rem_last - (ssize_t)rem_next) !=
                    op->data.cut.rem_off)
                FAIL_OP("rem_off %zd != %zd",
                        ((ssize_t)rem_last - (ssize_t)rem_next),
                        op->data.cut.rem_off);
            rem_last = rem_next;
            if (passed)
                break;
            else
                goto cleanup;
        case OP_TYPE_EMPTY:
            tlog_stream_empty(&stream);
            break;
        default:
            fprintf(stderr, "Unknown operation type: %d\n", op->type);
            exit(1);
        }
    }

#undef FAIL_OP

    if (rem_last != t.rem_out)
        FAIL("rem %zu != %zu", rem_last, t.rem_out);
    if (stream.txt_len != t.txt_len)
        FAIL("txt_len %zu != %zu", stream.txt_len, t.txt_len);
    if (stream.bin_len != t.bin_len)
        FAIL("bin_len %zu != %zu", stream.bin_len, t.bin_len);
    if ((meta_last - meta_buf) != (ssize_t)t.meta_len)
        FAIL("meta_len %zd != %zu", (meta_last - meta_buf), t.meta_len);

#define BUF_CMP(_n, _r, _e) \
    do {                                                        \
        if (memcmp(_r, _e, SIZE) != 0) {                        \
            fprintf(stderr, "%s: " #_n "_buf mismatch:\n", n);  \
            tlog_test_diff(stderr, _r, SIZE, _e, SIZE);         \
            passed = false;                                     \
        }                                                       \
    } while (0)
    BUF_CMP(txt, stream.txt_buf, t.txt_buf);
    BUF_CMP(bin, stream.bin_buf, t.bin_buf);
    BUF_CMP(meta, meta_buf, t.meta_buf);
#undef BUF_CMP

#undef FAIL
    fprintf(stderr, "%s: %s\n", n, (passed ? "PASS" : "FAIL"));

cleanup:
    tlog_stream_cleanup(&stream);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = test(#_name_token, (struct test){_struct_init_args}) && passed

#define OP_WRITE(_data_init_args...) \
    {.type = OP_TYPE_WRITE, .data = {.write = {_data_init_args}}}

#define OP_FLUSH(_data_init_args...) \
    {.type = OP_TYPE_FLUSH, .data = {.flush = {_data_init_args}}}

#define OP_CUT(_data_init_args...) \
    {.type = OP_TYPE_CUT, .data = {.cut = {_data_init_args}}}

#define OP_EMPTY \
    {.type = OP_TYPE_EMPTY}

    TEST(null,             .op_list = {});
    TEST(null_flushed,     .op_list = {
                                OP_FLUSH(0)
                            },
                            .rem_in = SIZE,
                            .rem_out = SIZE);
    TEST(null_cut,          .op_list = {
                                OP_CUT(0)
                            },
                            .rem_in = SIZE,
                            .rem_out = SIZE);
    TEST(null_emptied,     .op_list = {
                                OP_EMPTY
                            },
                            .rem_in = SIZE,
                            .rem_out = SIZE);

    TEST(one_byte,          .op_list = {
                                OP_WRITE(.buf = "A",
                                         .len_in = 1,
                                         .rem_off = 3)
                            },
                            .rem_in = 3,
                            .txt_buf = "A",
                            .txt_len = 1);
    TEST(one_byte_flushed,  .op_list = {
                                OP_WRITE(.buf = "A",
                                         .len_in = 1,
                                         .rem_off = 3),
                                OP_FLUSH(.meta_off = 2)
                            },
                            .rem_in = 3,
                            .txt_buf = "A",
                            .txt_len = 1,
                            .meta_buf = "<1",
                            .meta_len = 2);
    TEST(one_byte_cut,      .op_list = {
                                OP_WRITE(.buf = "A",
                                         .len_in = 1,
                                         .rem_off = 3),
                                OP_CUT(0)
                            },
                            .rem_in = 3,
                            .txt_buf = "A",
                            .txt_len = 1);
    TEST(one_byte_emptied,  .op_list = {
                                OP_WRITE(.buf = "A",
                                         .len_in = 1,
                                         .rem_off = 3),
                                OP_EMPTY
                            },
                            .rem_in = 3,
                            .txt_buf = "A");

    TEST(invalid_byte,          .op_list = {
                                    OP_WRITE(.buf = {0xff},
                                             .len_in = 1,
                                             .rem_off = 10),
                                    OP_FLUSH(.meta_off = 4)
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 10,
                                .txt_buf = "�",
                                .txt_len = 3,
                                .bin_buf = "255",
                                .bin_len = 3,
                                .meta_buf = "[1/1",
                                .meta_len = 4);

    TEST(3_invalid_1_valid,     .op_list = {
                                    OP_WRITE(.buf = {0xf0, 0x9d, 0x84, 'X'},
                                             .len_in = 4,
                                             .rem_off = 21,
                                             .meta_off = 4),
                                    OP_FLUSH(.meta_off = 2)
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 21,
                                .txt_buf = "�X",
                                .txt_len = 4,
                                .bin_buf = "240,157,132",
                                .bin_len = 11,
                                .meta_buf = "[1/3<1",
                                .meta_len = 6);

    TEST(split_valid,           .op_list = {
                                    OP_WRITE(.buf = {0xf0, 0x9d},
                                             .len_in = 2),
                                    OP_WRITE(.buf = {0x84, 0x9e},
                                             .len_in = 2,
                                             .rem_off = 6),
                                    OP_FLUSH(.meta_off = 2)
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 6,
                                .txt_buf = {0xf0, 0x9d, 0x84, 0x9e},
                                .txt_len = 4,
                                .meta_buf = "<1",
                                .meta_len = 2);

    TEST(split_invalid,         .op_list = {
                                    OP_WRITE(.buf = {0xf0, 0x9d},
                                             .len_in = 2),
                                    OP_WRITE(.buf = {0x84, 'X'},
                                             .len_in = 2,
                                             .rem_off = 21,
                                             .meta_off = 4),
                                    OP_FLUSH(.meta_off = 2)
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 21,
                                .txt_buf = "�X",
                                .txt_len = 4,
                                .bin_buf = "240,157,132",
                                .bin_len = 11,
                                .meta_buf = "[1/3<1",
                                .meta_len = 6);

    TEST(cut_incomplete,        .op_list = {
                                    OP_WRITE(.buf = {'X', 0xf0, 0x9d},
                                             .len_in = 3,
                                             .rem_off = 3),
                                    OP_CUT(.rem_off = 14,
                                             .meta_off = 2),
                                    OP_FLUSH(.meta_off = 4)
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 17,
                                .txt_buf = "X�",
                                .txt_len = 4,
                                .bin_buf = "240,157",
                                .bin_len = 7,
                                .meta_buf = "<1[1/2",
                                .meta_len = 6);

    TEST(two_flushes,           .op_list = {
                                    OP_WRITE(.buf = "A",
                                             .len_in = 1,
                                             .rem_off = 3),
                                    OP_FLUSH(.meta_off = 2),
                                    OP_WRITE(.buf = "B",
                                             .len_in = 1,
                                             .rem_off = 3),
                                    OP_FLUSH(.meta_off = 2),
                                },
                                .rem_in = SIZE,
                                .rem_out = SIZE - 6,
                                .txt_buf = "AB",
                                .txt_len = 2,
                                .meta_buf = "<1<1",
                                .meta_len = 4);

    return !passed;
}
