/*
 * Tlog source test functions.
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

#ifndef _TLOG_TEST_SOURCE_H
#define _TLOG_TEST_SOURCE_H

#include <tlog/pkt.h>

enum tlog_test_source_op_type {
    TLOG_TEST_SOURCE_OP_TYPE_NONE,
    TLOG_TEST_SOURCE_OP_TYPE_READ,
    TLOG_TEST_SOURCE_OP_TYPE_LOC_GET,
    TLOG_TEST_SOURCE_OP_TYPE_NUM
};

extern const char *tlog_test_source_op_type_to_str(
                        enum tlog_test_source_op_type type);

struct tlog_test_source_op_data_loc_get {
    size_t exp_loc;
};

struct tlog_test_source_op_data_read {
    int             exp_grc;
    struct tlog_pkt exp_pkt;
};

struct tlog_test_source_op {
    enum tlog_test_source_op_type type;
    union {
        struct tlog_test_source_op_data_loc_get loc_get;
        struct tlog_test_source_op_data_read    read;
    } data;
};

struct tlog_test_source {
    const char                 *input;
    const char                 *hostname;
    const char                 *username;
    unsigned int                session_id;
    size_t                      io_size;
    struct tlog_test_source_op  op_list[16];
};

extern bool tlog_test_source(const char *name,
                             const struct tlog_test_source test);

#endif /* _TLOG_TEST_SOURCE_H */
