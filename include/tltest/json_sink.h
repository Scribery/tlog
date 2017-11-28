/**
 * @file
 * @brief Sink test functions.
 *
 * Functions and types specific to sink testing.
 */
/*
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

#ifndef _TLOG_TEST_JSON_SINK_H
#define _TLOG_TEST_JSON_SINK_H

#include <tlog/pkt.h>

enum tlog_test_json_sink_op_type {
    TLOG_TEST_JSON_SINK_OP_TYPE_NONE,
    TLOG_TEST_JSON_SINK_OP_TYPE_WRITE,
    TLOG_TEST_JSON_SINK_OP_TYPE_FLUSH,
    TLOG_TEST_JSON_SINK_OP_TYPE_CUT,
    TLOG_TEST_JSON_SINK_OP_TYPE_NUM
};

extern const char* tlog_test_json_sink_op_type_to_str(
                        enum tlog_test_json_sink_op_type type);

struct tlog_test_json_sink_op {
    enum tlog_test_json_sink_op_type type;
    union {
        struct tlog_pkt write;
    } data;
};

#define TLOG_TEST_JSON_SINK_OP_NONE \
    ((struct tlog_test_json_sink_op)                \
        {.type = TLOG_TEST_JSON_SINK_OP_TYPE_NONE})

#define TLOG_TEST_JSON_SINK_OP_WRITE(_pkt) \
    ((struct tlog_test_json_sink_op){               \
        .type = TLOG_TEST_JSON_SINK_OP_TYPE_WRITE,  \
        .data.write = _pkt                          \
    })

#define TLOG_TEST_JSON_SINK_OP_FLUSH \
    ((struct tlog_test_json_sink_op)                    \
        {.type = TLOG_TEST_JSON_SINK_OP_TYPE_FLUSH})

#define TLOG_TEST_JSON_SINK_OP_CUT \
    ((struct tlog_test_json_sink_op)                \
        {.type = TLOG_TEST_JSON_SINK_OP_TYPE_CUT})

struct tlog_test_json_sink_input {
    const char                     *hostname;
    const char                     *recording;
    const char                     *username;
    const char                     *terminal;
    unsigned int                    session_id;
    size_t                          chunk_size;
    struct tlog_test_json_sink_op   op_list[16];
};

extern bool tlog_test_json_sink_run(
                const char                                 *name,
                const struct tlog_test_json_sink_input     *input,
                char                                      **pres_output_buf,
                size_t                                     *pres_output_len);

struct tlog_test_json_sink {
    struct tlog_test_json_sink_input     input;
    const char                     *output;
};

extern bool tlog_test_json_sink(const char *file, int line, const char *name,
                                const struct tlog_test_json_sink test);

#endif /* _TLOG_TEST_JSON_SINK_H */
