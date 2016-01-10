/**
 * @file
 * @brief Transaction state.
 *
 * Transaction state keeps track of transaction level and stack depth.
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

#ifndef _TLOG_TRX_STATE_H
#define _TLOG_TRX_STATE_H

#include <stdint.h>
#include <tlog/trx_level.h>

/** Transaction state (level and frame) */
typedef unsigned int tlog_trx_state;

/** LSB of transaction level in a transaction state */
#define TLOG_TRX_STATE_LEVEL_LSB \
    ((sizeof(tlog_trx_state) * 8) - TLOG_TRX_LEVEL_BITS)

/** Mask of transaction level in a transaction state */
#define TLOG_TRX_STATE_LEVEL_MASK \
    (TLOG_TRX_LEVEL_MAX << TLOG_TRX_STATE_LEVEL_LSB)

/** Mask of transaction frame in a transaction state */
#define TLOG_TRX_STATE_DEPTH_MASK \
    (~TLOG_TRX_STATE_LEVEL_MASK)

/** Get transaction level from a transaction state */
#define TLOG_TRX_STATE_GET_LEVEL(_trx) \
    ((_trx) >> TLOG_TRX_STATE_LEVEL_LSB)

/** Get transaction stack depth from a transaction state */
#define TLOG_TRX_STATE_GET_DEPTH(_trx) \
    ((_trx) & TLOG_TRX_STATE_DEPTH_MASK)

/** Maximum stack depth which can be stored in a transaction state */
#define TLOG_TRX_STATE_MAX_DEPTH \
    ((~(tlog_trx_state)0) >> TLOG_TRX_LEVEL_BITS)

/** Root transaction state initializer */
#define TLOG_TRX_STATE_ROOT   0

/** Sub-transaction state initializer */
#define TLOG_TRX_STATE_SUB(_parent) \
    ({                                                                  \
        tlog_trx_state _level = TLOG_TRX_STATE_GET_LEVEL(_parent) + 1;  \
        assert(_level <= TLOG_TRX_LEVEL_MAX);                           \
        (_level << TLOG_TRX_STATE_LEVEL_LSB);                           \
    })

#endif /* _TLOG_TRX_STATE_H */
