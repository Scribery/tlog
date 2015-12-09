/*
 * Tlog transaction.
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

#ifndef _TLOG_TRX_H
#define _TLOG_TRX_H

#include <stdint.h>
#include <tlog/grc.h>
#include <tlog/rc.h>

/** Transaction depth mask */
typedef unsigned int tlog_trx_mask;

/** Minimum transaction depth */
#define TLOG_TRX_DEPTH_MIN  0

/** Maximum transaction depth */
#define TLOG_TRX_DEPTH_MAX  (sizeof(tlog_trx_mask) == 4 ? 31 : 63)

/** Number of bits required to store transaction depth */
#define TLOG_TRX_DEPTH_BITS (sizeof(tlog_trx_mask) == 4 ? 5 : 6)

/** Transaction state (transaction and stack depth) */
typedef unsigned int tlog_trx;

/** LSB of transaction depth in transaction state */
#define TLOG_TRX_DEPTH_LSB  ((sizeof(tlog_trx) * 8) - TLOG_TRX_DEPTH_BITS)

/** Mask of transaction depth in transaction state */
#define TLOG_TRX_DEPTH_MASK (TLOG_TRX_DEPTH_MAX << TLOG_TRX_DEPTH_BITS)

/** Mask of transaction stack in transaction state */
#define TLOG_TRX_STACK_MASK (~TLOG_TRX_DEPTH_MASK)

/** Get transaction depth */
#define TLOG_TRX_GET_DEPTH(_trx) ((_trx) >> TLOG_TRX_DEPTH_LSB)

/** Get transaction stack */
#define TLOG_TRX_GET_STACK(_trx) ((_trx) & TLOG_TRX_STACK_MASK)

/** Root transaction initializer */
#define TLOG_TRX_ROOT   0

/** Sub-transaction initializer */
#define TLOG_TRX_SUB(_parent)  \
    ((TLOG_TRX_GET_DEPTH(_parent) + 1) << TLOG_TRX_DEPTH_LSB)

/** Transaction data store name */
#define TLOG_TRX_STORE_NAME(_type_token) \
    _type_token##_trx_store

/** Transaction data store type signature */
#define TLOG_TRX_STORE_SIG(_type_token) \
    struct TLOG_TRX_STORE_NAME(_type_token)

/** Transaction data store member variable declaration */
#define TLOG_TRX_STORE_VAR(_type_token, _name_token) \
    typeof(((struct _type_token *)NULL)->_name_token) _name_token

/** Transaction data store object declaration */
#define TLOG_TRX_STORE_OBJ(_obj_type_token, _name_token) \
    TLOG_TRX_STORE_SIG(_obj_type_token) _name_token

/** Transaction data store proxy declaration */
#define TLOG_TRX_STORE_PROXY(_name_token) \
    void *_name_token

/** Transaction data transfer direction */
enum tlog_trx_xfr_dir {
    TLOG_TRX_XFR_DIR_BACKUP,
    TLOG_TRX_XFR_DIR_RESTORE
};

/** Transaction data transfer function name */
#define TLOG_TRX_XFR_NAME(_type_token) _type_token##_trx_xfr

/** Transaction data transfer function signature */
#define TLOG_TRX_XFR_SIG(_type_token) \
    tlog_grc TLOG_TRX_XFR_NAME(_type_token)(enum tlog_trx_xfr_dir dir,  \
                                            void *abstract_object,      \
                                            void *abstract_store)

/** Transaction data transfer function prologue */
#define TLOG_TRX_XFR_PROLOGUE(_type_token) \
    struct _type_token *object =                            \
        (struct _type_token *)abstract_object;              \
    TLOG_TRX_STORE_SIG(_type_token) *store =                \
        (TLOG_TRX_STORE_SIG(_type_token) *)abstract_store;  \
    tlog_grc grc;

/** Transaction data transfer function epilogue */
#define TLOG_TRX_XFR_EPILOGUE \
    grc = TLOG_RC_OK;           \
    epilogue:                       \
    return grc

/**
 * Transaction data transfer function prototype.
 *
 * @param dir               Transfer direction.
 * @param abstract_object   The object to backup/restore.
 * @param abstract_store    The store to backup to/restore from.
 *
 * @return Global return code.
 */
typedef tlog_grc (*tlog_trx_xfr_fn)(enum tlog_trx_xfr_dir dir,
                                    void *abstract_object,
                                    void *abstract_store);

/** An object's transaction management interface */
struct tlog_trx_iface {
    size_t              store_size; /**< Store object size */
    ssize_t             mask_off;   /**< Offset of transaction mask */
    tlog_trx_xfr_fn     xfr;        /**< Transaction data transfer function */
};

/** A type's transaction interface name */
#define TLOG_TRX_IFACE_NAME(_type_token) _type_token##_trx_iface

/** An object's transaction instance */
struct tlog_trx_obj {
    struct tlog_trx_iface  *iface;
    void                   *object;
    void                   *store;
};

/** Transfer transaction data of a variable member */
#define TLOG_TRX_XFR_VAR(_member_name_token) \
    do {                                                            \
        if (dir == TLOG_TRX_XFR_DIR_BACKUP)                         \
            store->_member_name_token = object->_member_name_token; \
        else                                                        \
            object->_member_name_token = store->_member_name_token; \
    } while (0)

/** Transfer transaction data of an object member */
#define TLOG_TRX_XFR_OBJ(_type_token, _member_name_token) \
    do {                                                            \
        grc = _type_token##_trx_xfr(dir,                            \
                                    &object->_member_name_token,    \
                                    &store->_member_name_token);    \
        if (grc != TLOG_RC_OK)                                      \
            goto epilogue;                                          \
    } while (0)

/** Transfer transaction data of a proxy member */
#define TLOG_TRX_XFR_PROXY(_type_token, _member_name_token) \
    if (dir == TLOG_TRX_XFR_DIR_BACKUP) {                           \
        store->_member_name_token =                                 \
            malloc(object->_member_name_token->store_size);         \
        if (store->_member_name_token == NULL) {                    \
            grc = TLOG_GRC_ERRNO;                                   \
            goto epilogue;                                          \
        }                                                           \
        grc = object->_member_name_token->xfr(dir, object, store);  \
        if (grc != TLOG_RC_OK)                                      \
            goto epilogue;                                          \
    } else {                                                        \
        grc = object->_member_name_token->xfr(dir, object, store);  \
        if (grc != TLOG_RC_OK)                                      \
            goto epilogue;                                          \
        free(store->_member_name_token);                            \
    }

/**
 * Define a transaction frame object.
 */
#define TLOG_TRX_FRAME_OBJ(_type_token, _ptr) \
    {.iface = &TLOG_TRX_IFACE_NAME(_type_token), .object = _ptr}

/**
 * Define a transaction frame proxy.
 */
#define TLOG_TRX_FRAME_PROXY(_ptr) \
    {.iface = &_ptr->trx_iface, .object = _ptr}

/**
 * Define the transaction frame.
 */
#define TLOG_TRX_FRAME_DEF(_obj...) \
    struct tlog_trx_obj _trx_frame[] = {_obj}

/**
 * Begin or continue a transaction for an object of a specific type, backing
 * the object up, if the transaction is beginning.
 *
 * @param _trx          Transaction lvalue.
 */
#define TLOG_TRX_BEGIN(_trx) \
    ({                                                                  \
        tlog_grc grc;                                                   \
        if ((_trx)++ == 0) {                                            \
            size_t i;                                                   \
            struct tlog_trx_obj *obj;                                   \
            for (i = 0, i < TLOG_ARRAY_SIZE(_trx_frame), i++) {         \
                obj = _trx_frame[i];                                    \
                obj->store = alloca(obj->iface->store_size);            \
                grc = _type_token##_trx_xfr(TLOG_TRX_XFR_DIR_BACKUP,    \
                                            obj->object, obj->store);   \
                if (grc != TLOG_RC_OK)                                  \
                    break;                                              \
            }                                                           \
        } else {                                                        \
            grc = TLOG_RC_OK;                                           \
        }                                                               \
        grc;                                                            \
    })

/**
 * Terminate or abort a transaction for an object of a specific type,
 * restoring the object, if the transaction is aborted.
 *
 * @param _trx          Transaction lvalue.
 * @param _type_token   The token of the type for which the transaction
 *                      terminates/aborts.
 * @param _object       The object to terminate/abort the transaction for.
 */
#define TLOG_TRX_ABORT(_trx, _type_token, _object) \
    ({                                                                  \
        assert((_trx) != 0);                                            \
        if (--(_trx) == 0) {                                            \
            size_t i;                                                   \
            struct tlog_trx_obj *obj;                                   \
            for (i = 0, i < TLOG_ARRAY_SIZE(_trx_frame), i++) {         \
                obj = _trx_frame[i];                                    \
                grc = _type_token##_trx_xfr(TLOG_TRX_XFR_DIR_RESTORE,   \
                                            obj->object, obj->store);   \
                if (grc != TLOG_RC_OK)                                  \
                    break;                                              \
            }                                                           \
        } else {                                                        \
            grc = TLOG_RC_OK;                                           \
        }                                                               \
        grc;                                                            \
    })

/**
 * Terminate or commit a transaction.
 *
 * @param _trx          Transaction lvalue.
 */
#define TLOG_TRX_COMMIT(_trx) \
    do {                        \
        assert((_trx) != 0);    \
        --(_trx);               \
    } while (0)

#endif /* _TLOG_TRX_H */
