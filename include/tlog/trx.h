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

/** Transaction */
typedef size_t tlog_trx;

/** Empty transaction initializer */
#define TLOG_TRX_INIT   0

/** Transaction data transfer direction */
enum tlog_trx_xfr_dir {
    TLOG_TRX_XFR_DIR_BACKUP,
    TLOG_TRX_XFR_DIR_RESTORE
};

/** Transaction data store name */
#define TLOG_TRX_STORE_NAME(_type_token) \
    _type_token##_trx_store

/** Transaction data store type signature */
#define TLOG_TRX_STORE_SIG(_type_token) \
    struct TLOG_TRX_STORE_NAME(_type_token)

/** Transaction data transfer function signature */
#define TLOG_TRX_XFR_SIG(_type_token) \
    void _type_token##_trx_xfr(enum tlog_trx_xfr_dir dir,               \
                               struct _type_token *object,              \
                               TLOG_TRX_STORE_SIG(_type_token) *store)

/** Transfer transaction data of a variable member */
#define TLOG_TRX_XFR_VAR(_token) \
    do {                                    \
        if (dir == TLOG_TRX_XFR_DIR_BACKUP) \
            store->_token = object->_token; \
        else                                \
            object->_token = store->_token; \
    } while (0)

/** Transfer transaction data of an object member */
#define TLOG_TRX_XFR_OBJ(_type_token, _name_token) \
    _type_token##_trx_xfr(dir, &object->_name_token, &store->_name_token)

/**
 * Declare the transaction store variable for a type.
 *
 * @param _type_token   The token of the type to be stored.
 */
#define TLOG_TRX_STORE_DECL(_type_token) \
    TLOG_TRX_STORE_SIG(_type_token) TLOG_TRX_STORE_NAME(_type_token);

/**
 * Begin or continue a transaction for an object of a specific type, backing
 * the object up, if the transaction is beginning.
 *
 * @param _ptrx         Pointer to the transaction.
 * @param _type_token   The token of the type for which the transaction
 *                      begins/continues.
 * @param _object       The object to begin the transaction for.
 */
#define TLOG_TRX_BEGIN(_ptrx, _type_token, _object) \
    do {                                                                \
        if ((*(_ptrx))++ == 0)                                          \
            _type_token##_trx_xfr(TLOG_TRX_XFR_DIR_BACKUP,              \
                                  _object,                              \
                                  &TLOG_TRX_STORE_NAME(_type_token));   \
    } while (0)

/**
 * Terminate or abort a transaction for an object of a specific type,
 * restoring the object, if the transaction is aborted.
 *
 * @param _ptrx         Pointer to the transaction.
 * @param _type_token   The token of the type for which the transaction
 *                      terminates/aborts.
 * @param _object       The object to terminate/abort the transaction for.
 */
#define TLOG_TRX_ABORT(_ptrx, _type_token, _object) \
    do {                                                                \
        assert(*(_ptrx) != 0);                                          \
        if (--(*(_ptrx)) == 0)                                          \
            _type_token##_trx_xfr(TLOG_TRX_XFR_DIR_RESTORE,             \
                                  _object,                              \
                                  &TLOG_TRX_STORE_NAME(_type_token));   \
    } while (0)

/**
 * Terminate or commit a transaction.
 *
 * @param _ptrx         Pointer to the transaction.
 */
#define TLOG_TRX_COMMIT(_ptrx) \
    do {                            \
        assert(*(_ptrx) != 0);      \
        --*(_ptrx);                 \
    } while (0)

#endif /* _TLOG_TRX_H */
