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

#ifndef _TLTEST_JSON_SINK_H
#define _TLTEST_JSON_SINK_H

#include <tlog/pkt.h>

enum tltest_json_sink_op_type {
    TLTEST_JSON_SINK_OP_TYPE_NONE,
    TLTEST_JSON_SINK_OP_TYPE_WRITE,
    TLTEST_JSON_SINK_OP_TYPE_FLUSH,
    TLTEST_JSON_SINK_OP_TYPE_CUT,
    TLTEST_JSON_SINK_OP_TYPE_NUM
};

extern const char* tltest_json_sink_op_type_to_str(
                        enum tltest_json_sink_op_type type);

struct tltest_json_sink_op {
    enum tltest_json_sink_op_type type;
    union {
        struct tlog_pkt write;
    } data;
};

#define TLTEST_JSON_SINK_OP_NONE \
    ((struct tltest_json_sink_op)                   \
        {.type = TLTEST_JSON_SINK_OP_TYPE_NONE})

#define TLTEST_JSON_SINK_OP_WRITE(_pkt) \
    ((struct tltest_json_sink_op){              \
        .type = TLTEST_JSON_SINK_OP_TYPE_WRITE, \
        .data.write = _pkt                      \
    })

#define TLTEST_JSON_SINK_OP_FLUSH \
    ((struct tltest_json_sink_op)                   \
        {.type = TLTEST_JSON_SINK_OP_TYPE_FLUSH})

#define TLTEST_JSON_SINK_OP_CUT \
    ((struct tltest_json_sink_op)               \
        {.type = TLTEST_JSON_SINK_OP_TYPE_CUT})

struct tltest_json_sink_input {
    const char                 *hostname;
    const char                 *recording;
    const char                 *username;
    const char                 *terminal;
    unsigned int                session_id;
    size_t                      chunk_size;
    struct tltest_json_sink_op  op_list[16];
};

extern bool tltest_json_sink_run(
                const char                             *name,
                const struct tltest_json_sink_input    *input,
                char                                  **pres_output_buf,
                size_t                                 *pres_output_len);

struct tltest_json_sink {
    struct tltest_json_sink_input   input;
    const char                     *output;
};

extern bool tltest_json_sink(const char *file, int line, const char *name,
                             const struct tltest_json_sink test);

#endif /* _TLTEST_JSON_SINK_H */
