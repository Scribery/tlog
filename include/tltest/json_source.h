/**
 * @file
 * @brief Source test functions.
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

#ifndef _TLTEST_JSON_SOURCE_H
#define _TLTEST_JSON_SOURCE_H

#include <tlog/pkt.h>

enum tltest_json_source_op_type {
    TLTEST_JSON_SOURCE_OP_TYPE_NONE,
    TLTEST_JSON_SOURCE_OP_TYPE_READ,
    TLTEST_JSON_SOURCE_OP_TYPE_LOC_GET,
    TLTEST_JSON_SOURCE_OP_TYPE_NUM
};

extern const char *tltest_json_source_op_type_to_str(
                        enum tltest_json_source_op_type type);

struct tltest_json_source_op_data_loc_get {
    size_t exp_loc;
};

struct tltest_json_source_op_data_read {
    int             exp_grc;
    struct tlog_pkt exp_pkt;
};

struct tltest_json_source_op {
    enum tltest_json_source_op_type  type;
    union {
        struct tltest_json_source_op_data_loc_get   loc_get;
        struct tltest_json_source_op_data_read      read;
    } data;
};

#define TLTEST_JSON_SOURCE_OP_NONE \
    ((struct tltest_json_source_op){            \
        .type = TLTEST_JSON_SOURCE_OP_TYPE_NONE \
    })

#define TLTEST_JSON_SOURCE_OP_LOC_GET(_exp_loc) \
    ((struct tltest_json_source_op){                \
        .type = TLTEST_JSON_SOURCE_OP_TYPE_LOC_GET, \
        .data = {.loc_get = {.exp_loc = _exp_loc}}  \
    })

#define TLTEST_JSON_SOURCE_OP_READ(_exp_grc, _exp_pkt) \
    ((struct tltest_json_source_op){                                    \
        .type = TLTEST_JSON_SOURCE_OP_TYPE_READ,                        \
        .data = {.read = {.exp_grc = _exp_grc, .exp_pkt = _exp_pkt}}    \
    })

struct tltest_json_source_output {
    const char                     *hostname;
    const char                     *username;
    const char                     *terminal;
    unsigned int                    session_id;
    size_t                          io_size;
    struct tltest_json_source_op    op_list[16];
};

extern bool tltest_json_source_run(
                    const char                             *name,
                    const char                             *input_buf,
                    size_t                                  input_len,
                    const struct tltest_json_source_output *output);

struct tltest_json_source {
    const char                         *input;
    struct tltest_json_source_output    output;
};

extern bool tltest_json_source(const char *file, int line, const char *name,
                               const struct tltest_json_source test);

#endif /* _TLTEST_JSON_SOURCE_H */
