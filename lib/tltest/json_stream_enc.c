/*
 * Tlog_json_stream_enc_* function test module.
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
#include <string.h>
#include <tlog/misc.h>
#include <tltest/misc.h>
#include <tltest/json_stream_enc.h>

#define BOOL_STR(_b) ((_b) ? "true" : "false")

TLOG_TRX_BASIC_STORE_SIG(test_meta) {
    size_t rem;
};

struct test_meta {
    struct tlog_json_dispatcher dispatcher;
    size_t                      rem;
    struct tlog_trx_iface       trx_iface;
    TLOG_TRX_BASIC_MEMBERS(test_meta);
};

static
TLOG_TRX_BASIC_ACT_SIG(test_meta)
{
    TLOG_TRX_BASIC_ACT_PROLOGUE(test_meta);
    TLOG_TRX_BASIC_ACT_ON_VAR(rem);
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
    assert(tlog_json_dispatcher_is_valid(dispatcher));
    assert(ptr != NULL || len == 0);
    (void)dispatcher;
    (void)ptr;
    (void)len;
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
    assert(meta != NULL);

    memset(meta, 0, sizeof(*meta));
    meta->trx_iface = TLOG_TRX_BASIC_IFACE(test_meta);
    tlog_json_dispatcher_init(&meta->dispatcher,
                              test_meta_dispatcher_advance,
                              test_meta_dispatcher_reserve,
                              test_meta_dispatcher_write,
                              meta,
                              &meta->trx_iface);
    meta->rem = rem;
}

bool
tltest_json_stream_enc(const char *file, int line, const char *n,
                       const struct tltest_json_stream_enc t)
{
    bool passed = true;
    uint8_t obuf[TLTEST_JSON_STREAM_ENC_BUF_SIZE] = {0,};
    struct test_meta meta;
    size_t olen = t.olen_in;
    size_t irun = t.irun_in;
    size_t idig = t.idig_in;
    size_t orem;
    bool fit;

    test_meta_init(&meta, t.orem_in);

#define FAIL(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, "%s: " _fmt "\n", n, ##_args);  \
        passed = false;                                 \
    } while (0)

#define CMP_SIZE(_name) \
    do {                                                       \
        if (_name != t._name##_out) {                          \
            FAIL(#_name " %zu != %zu", _name, t._name##_out);  \
        }                                                      \
    } while (0)

#define CMP_BOOL(_name) \
    do {                                                        \
        if (_name != t._name##_out) {                           \
            FAIL(#_name " %s != %s",                            \
                 BOOL_STR(_name), BOOL_STR(t._name##_out));     \
        }                                                       \
    } while (0)

    fit = t.func(TLOG_TRX_STATE_ROOT, &meta.dispatcher,
                 obuf, &olen, &irun, &idig, t.ibuf_in, t.ilen_in);
    orem = meta.rem;
    CMP_BOOL(fit);
    CMP_SIZE(orem);
    CMP_SIZE(olen);
    CMP_SIZE(irun);
    CMP_SIZE(idig);

    if (memcmp(obuf, t.obuf_out, TLTEST_JSON_STREAM_ENC_BUF_SIZE) != 0) {
        fprintf(stderr, "%s: obuf mismatch:\n", n);
        tltest_diff(stderr,
                    obuf, TLTEST_JSON_STREAM_ENC_BUF_SIZE,
                    t.obuf_out, TLTEST_JSON_STREAM_ENC_BUF_SIZE);
        passed = false;
    }

#undef CMP_SIZE
#undef CMP_BOOL
#undef FAIL
    fprintf(stderr, "%s %s:%d %s\n", (passed ? "PASS" : "FAIL"),
            file, line, n);

    return passed;
}
