/**
 * @file
 * @brief Basic transaction support implementation.
 *
 * An implementation of basic transaction storage and interface.
 * The transaction storage for all possible transaction levels is supposed to
 * be located within the object itself as an array of specially defined types,
 * along with a mask describing which level's backup is valid. This is
 * accompanied by macros simplifying definition of a transaction data acting
 * function.
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

#ifndef _TLOG_TRX_BASIC_H
#define _TLOG_TRX_BASIC_H

#include <tlog/trx_level.h>
#include <tlog/trx_act.h>
#include <tlog/trx_iface.h>

/** Transaction level mask */
typedef unsigned int tlog_trx_basic_mask;

/** Transaction data store name */
#define TLOG_TRX_BASIC_STORE_NAME(_type_token) \
    _type_token##_trx_store

/** Transaction data store type signature */
#define TLOG_TRX_BASIC_STORE_SIG(_type_token) \
    struct TLOG_TRX_BASIC_STORE_NAME(_type_token)

/** A type's transaction-specific members */
#define TLOG_TRX_BASIC_MEMBERS(_type_token) \
    tlog_trx_basic_mask                     \
        trx_store_mask;                     \
    TLOG_TRX_BASIC_STORE_SIG(_type_token)   \
        trx_store_list[TLOG_TRX_LEVEL_NUM]

/** Transaction data action function name */
#define TLOG_TRX_BASIC_ACT_NAME(_type_token) _type_token##_trx_act

/** Transaction data action function signature */
#define TLOG_TRX_BASIC_ACT_SIG(_type_token) \
    void TLOG_TRX_BASIC_ACT_NAME(_type_token)(                          \
                                    tlog_trx_level level,               \
                                    enum tlog_trx_act_type act_type,    \
                                    void *abstract_object)

/** Transaction data action function prologue */
#define TLOG_TRX_BASIC_ACT_PROLOGUE(_type_token) \
    struct _type_token *object =                                                \
        (struct _type_token *)abstract_object;                                  \
    assert(level < sizeof(object->trx_store_mask) * 8);                         \
    assert(level < TLOG_ARRAY_SIZE(object->trx_store_list));                    \
    tlog_trx_basic_mask mask = (1 << level);                                    \
    if ((act_type == TLOG_TRX_ACT_TYPE_BACKUP) !=                               \
        (!(object->trx_store_mask & mask)))                                     \
        return;                                                                 \
    object->trx_store_mask = (object->trx_store_mask & ~mask) |                 \
                             ((act_type == TLOG_TRX_ACT_TYPE_BACKUP) << level); \
    TLOG_TRX_BASIC_STORE_SIG(_type_token) *store =                              \
        &object->trx_store_list[level];

/** Act on transaction data of a variable member */
#define TLOG_TRX_BASIC_ACT_ON_VAR(_name) \
    do {                                                \
        if (act_type == TLOG_TRX_ACT_TYPE_BACKUP)       \
            store->_name = object->_name;               \
        else if (act_type == TLOG_TRX_ACT_TYPE_RESTORE) \
            object->_name = store->_name;               \
    } while (0)

/** Act on transaction data of an embedded object member */
#define TLOG_TRX_BASIC_ACT_ON_OBJ(_name) \
    object->_name.trx_iface.act(level, act_type, &object->_name)

/** Act on transaction data of an object pointer member */
#define TLOG_TRX_BASIC_ACT_ON_PTR(_name) \
    object->_name->trx_iface.act(level, act_type, object->_name)

/** Basic transaction support interface initializer */
#define TLOG_TRX_BASIC_IFACE(_type_token) \
    (struct tlog_trx_iface){.act = TLOG_TRX_BASIC_ACT_NAME(_type_token)}

#endif /* _TLOG_TRX_BASIC_H */
