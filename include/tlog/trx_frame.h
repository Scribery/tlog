/**
 * @file
 * @brief Transaction frame.
 *
 * Transaction frame keeps a list of objects participating in the transaction
 * along with their transaction interfaces. This list is used to
 * backup/restore/discard transaction data on transaction
 * beginning/abort/commit.
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

#ifndef _TLOG_TRX_FRAME_H
#define _TLOG_TRX_FRAME_H

#include <tlog/trx_state.h>

/** A transaction frame slot */
struct tlog_trx_frame_slot {
    struct tlog_trx_iface  *iface;  /**< Object's transaction interface */
    void                   *object; /**< Abstract object pointer */
};

/**
 * Frame slot initializer.
 */
#define TLOG_TRX_FRAME_SLOT(_obj) \
    {                               \
        .iface  = &_obj->trx_iface, \
        .object = _obj              \
    }

/**
 * Define the scope's transaction frame.
 */
#define TLOG_TRX_FRAME_DEF(_slot...) \
    const struct tlog_trx_frame_slot _trx_frame[] = {_slot}

/**
 * Define the scope's single-slot frame
 */
#define TLOG_TRX_FRAME_DEF_SINGLE(_obj) \
    TLOG_TRX_FRAME_DEF(TLOG_TRX_FRAME_SLOT(_obj))

/**
 * Execute an action on transaction frame within scope.
 *
 * @param \_trx         Transaction state lvalue.
 * @param \_act_type    Action type.
 */
#define TLOG_TRX_FRAME_ACT(_trx, _act_type) \
    do {                                                        \
        size_t _i;                                              \
        const struct tlog_trx_frame_slot *_slot;                \
        for (_i = 0; _i < TLOG_ARRAY_SIZE(_trx_frame); _i++) {  \
            _slot = &_trx_frame[_i];                            \
            _slot->iface->act(TLOG_TRX_STATE_GET_LEVEL(_trx),   \
                              _act_type,                        \
                              _slot->object);                   \
        }                                                       \
    } while (0)

/**
 * Begin or continue a transaction for transaction frame within scope, backing
 * the objects up, if the transaction is beginning.
 *
 * @param \_trx     Transaction state lvalue.
 */
#define TLOG_TRX_FRAME_BEGIN(_trx) \
    do {                                                                    \
        assert(TLOG_TRX_STATE_GET_DEPTH(_trx) < TLOG_TRX_STATE_MAX_DEPTH);  \
        if (TLOG_TRX_STATE_GET_DEPTH(_trx) == 0) {                          \
            TLOG_TRX_FRAME_ACT(_trx, TLOG_TRX_ACT_TYPE_BACKUP);             \
        }                                                                   \
        (_trx)++;                                                           \
    } while (0)

/**
 * Abort a transaction for transaction frame within scope, restoring the
 * objects from backups, if the transaction is ending.
 *
 * @param \_trx     Transaction state lvalue.
 */
#define TLOG_TRX_FRAME_ABORT(_trx) \
    do {                                                            \
        assert(TLOG_TRX_STATE_GET_DEPTH(_trx) > 0);                 \
        (_trx)--;                                                   \
        if (TLOG_TRX_STATE_GET_DEPTH(_trx) == 0) {                  \
            TLOG_TRX_FRAME_ACT(_trx, TLOG_TRX_ACT_TYPE_RESTORE);    \
        }                                                           \
    } while (0)

/**
 * Commit a transaction for transaction frame within scope, discarding the
 * object backups, if the transaction is ending.
 *
 * @param \_trx     Transaction lvalue.
 */
#define TLOG_TRX_FRAME_COMMIT(_trx) \
    do {                                                            \
        assert(TLOG_TRX_STATE_GET_DEPTH(_trx) > 0);                 \
        (_trx)--;                                                   \
        if (TLOG_TRX_STATE_GET_DEPTH(_trx) == 0) {                  \
            TLOG_TRX_FRAME_ACT(_trx, TLOG_TRX_ACT_TYPE_DISCARD);    \
        }                                                           \
    } while (0)

#endif /* _TLOG_TRX_FRAME_H */
