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

/**
 * Declare the transaction store variable for a type.
 *
 * @param _type_token   The token of the type to be stored.
 */
#define TLOG_TRX_STORE_DECL(_type_token) \
    struct _type_token##_trx_store _type_token##_trx_store;

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
    do {                                                                    \
        if ((*(_ptrx))++ == 0)                                              \
            _type_token##_trx_backup(&_type_token##_trx_store, _object);    \
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
    do {                                                                    \
        assert(*(_ptrx) != 0);                                              \
        if (--(*(_ptrx)) == 0)                                              \
            _type_token##_trx_restore(&_type_token##_trx_store, _object);   \
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
