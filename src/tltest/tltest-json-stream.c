/*
 * Tlog tlog_json_stream test.
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
#include <tlog/json_stream.h>
#include <tltest/misc.h>
#include <tlog/misc.h>
#include <tlog/rc.h>

#define SIZE    TLOG_JSON_STREAM_SIZE_MIN

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

TLOG_TRX_BASIC_STORE_SIG(test_meta) {
    uint8_t                *ptr;
    size_t                  rem;
};

struct test_meta {
    struct tlog_json_dispatcher     dispatcher;
    uint8_t                         buf[SIZE];
    uint8_t                        *ptr;
    size_t                          rem;
    struct tlog_json_stream         stream;
    struct tlog_trx_iface           trx_iface;
    TLOG_TRX_BASIC_MEMBERS(test_meta);
};

static
TLOG_TRX_BASIC_ACT_SIG(test_meta)
{
    TLOG_TRX_BASIC_ACT_PROLOGUE(test_meta);
    TLOG_TRX_BASIC_ACT_ON_VAR(ptr);
    TLOG_TRX_BASIC_ACT_ON_VAR(rem);
    TLOG_TRX_BASIC_ACT_ON_OBJ(stream);
}

static bool
test_meta_dispatcher_reserve(struct tlog_json_dispatcher *dispatcher,
                             size_t len)
{
    struct test_meta *meta = TLOG_CONTAINER_OF(dispatcher,
                                               struct test_meta,
                                               dispatcher);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    if (len > meta->rem) {
        return false;
    }
    meta->rem -= len;
    return true;
}

static void
test_meta_dispatcher_write(struct tlog_json_dispatcher *dispatcher,
                           const uint8_t *ptr, size_t len)
{
    struct test_meta *meta = TLOG_CONTAINER_OF(dispatcher,
                                               struct test_meta,
                                               dispatcher);
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert((meta->ptr - meta->buf + len) <= sizeof(meta->buf) - meta->rem);

    memcpy(meta->ptr, ptr, len);
    meta->ptr += len;
}

static bool
test_meta_dispatcher_advance(tlog_trx_state trx,
                             struct tlog_json_dispatcher *dispatcher,
                             const struct timespec *ts)
{
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(ts != NULL);
    (void)trx;
    (void)dispatcher;
    (void)ts;
    return true;
}

static void
test_meta_init(struct test_meta *meta, size_t rem)
{
    tlog_grc grc;
    assert(meta != NULL);
    assert(rem <= sizeof(meta->buf));

    memset(meta, 0, sizeof(*meta));
    meta->trx_iface = TLOG_TRX_BASIC_IFACE(test_meta);
    tlog_json_dispatcher_init(&meta->dispatcher,
                              test_meta_dispatcher_advance,
                              test_meta_dispatcher_reserve,
                              test_meta_dispatcher_write,
                              meta, &meta->trx_iface);
    meta->ptr = meta->buf;
    meta->rem = rem;
    grc = tlog_json_stream_init(&meta->stream, &meta->dispatcher,
                                SIZE, '<', '[');
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed initializing the stream: %s\n",
                tlog_grc_strerror(grc));
        exit(1);
    }
}

static void
test_meta_cleanup(struct test_meta *meta)
{
    assert(meta != NULL);
    tlog_json_stream_cleanup(&meta->stream);
}

static bool
test(const char *file, int line, const char *n, const struct test t)
{
    bool passed = true;
    struct test_meta meta;
    const uint8_t *buf;
    size_t len;
    uint8_t *last_ptr;
    size_t last_rem;
    const struct op *op;
    struct timespec ts = {0, 0};

    assert(t.rem_in <= SIZE);

    test_meta_init(&meta, t.rem_in);
    last_ptr = meta.ptr;
    last_rem = meta.rem;

#define FAIL(_fmt, _args...) \
    do {                                                          \
        fprintf(stderr, "FAIL %s:%d %s" _fmt "\n", file, line,    \
                n, ##_args);                                      \
        passed = false;                                           \
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
            tlog_json_stream_write(TLOG_TRX_STATE_ROOT, &meta.stream, &ts,
                                   &buf, &len);
            if ((buf < op->data.write.buf) ||
                (buf - op->data.write.buf) !=
                    (ssize_t)(op->data.write.len_in -
                              op->data.write.len_out)) {
                FAIL_OP("off %zd != %zu",
                        (buf - op->data.write.buf),
                        (op->data.write.len_in - op->data.write.len_out));
            }
            if (len != op->data.write.len_out) {
                FAIL_OP("len %zu != %zu", len, op->data.write.len_out);
            }
            if ((meta.ptr - last_ptr) != op->data.write.meta_off) {
                FAIL_OP("meta_off %zd != %zd",
                        (meta.ptr - last_ptr), op->data.write.meta_off);
            }
            last_ptr = meta.ptr;
            if (((ssize_t)last_rem - (ssize_t)meta.rem) !=
                    op->data.write.rem_off) {
                FAIL_OP("rem_off %zd != %zd",
                        (last_rem - meta.rem), op->data.write.rem_off);
            }
            last_rem = meta.rem;
            if (passed) {
                break;
            } else {
                goto cleanup;
            }
        case OP_TYPE_FLUSH:
            tlog_json_stream_flush(&meta.stream);
            if ((meta.ptr - last_ptr) != op->data.flush.meta_off) {
                FAIL_OP("meta_off %zd != %zd",
                        (meta.ptr - last_ptr),
                        op->data.flush.meta_off);
            }
            last_ptr = meta.ptr;
            if (passed) {
                break;
            } else {
                goto cleanup;
            }
        case OP_TYPE_CUT:
            tlog_json_stream_cut(TLOG_TRX_STATE_ROOT, &meta.stream);
            if ((meta.ptr - last_ptr) != op->data.cut.meta_off) {
                FAIL_OP("meta_off %zd != %zd",
                        (meta.ptr - last_ptr), op->data.cut.meta_off);
            }
            last_ptr = meta.ptr;
            if (((ssize_t)last_rem - (ssize_t)meta.rem) !=
                    op->data.cut.rem_off) {
                FAIL_OP("rem_off %zd != %zd",
                        ((ssize_t)last_rem - (ssize_t)meta.rem),
                        op->data.cut.rem_off);
            }
            last_rem = meta.rem;
            if (passed) {
                break;
            } else {
                goto cleanup;
            }
        case OP_TYPE_EMPTY:
            tlog_json_stream_empty(&meta.stream);
            break;
        default:
            fprintf(stderr, "Unknown operation type: %d\n", op->type);
            exit(1);
        }
    }

#undef FAIL_OP

    if (last_rem != t.rem_out) {
        FAIL("rem %zu != %zu", last_rem, t.rem_out);
    }

#define BUF_CMP(_name, \
                _result_ptr, _result_len, _expected_ptr, _expected_len) \
    do {                                                                \
        if ((_result_len) != (_expected_len) ||                         \
            memcmp(_result_ptr, _expected_ptr, _expected_len) != 0) {   \
            fprintf(stderr, "%s: " #_name "_buf mismatch:\n", n);       \
            tltest_diff(stderr,                                         \
                        _result_ptr, _result_len,                       \
                        _expected_ptr, _expected_len);                  \
            passed = false;                                             \
        }                                                               \
    } while (0)
    BUF_CMP(txt, meta.stream.txt_buf, meta.stream.txt_len,
            t.txt_buf, t.txt_len);
    BUF_CMP(bin, meta.stream.bin_buf, meta.stream.bin_len,
            t.bin_buf, t.bin_len);
    BUF_CMP(meta,
            meta.buf, (size_t)(meta.ptr - meta.buf),
            t.meta_buf, t.meta_len);
#undef BUF_CMP

#undef FAIL
    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, n);

cleanup:
    test_meta_cleanup(&meta);
    return passed;
}

int
main(void)
{
    bool passed = true;

#define TEST(_name_token, _struct_init_args...) \
    passed = test(__FILE__, __LINE__, #_name_token,     \
                  (struct test){_struct_init_args}) && passed

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
                                OP_CUT(.meta_off = 0,
                                       .rem_off = 0)
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
                                OP_CUT(.meta_off = 0,
                                       .rem_off = 0)
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
