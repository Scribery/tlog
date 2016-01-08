/*
 * Tlog transaction data actions.
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

#ifndef _TLOG_TRX_ACT_H
#define _TLOG_TRX_ACT_H

#include <tlog/trx_level.h>

/** Transaction data action type */
enum tlog_trx_act_type {
    TLOG_TRX_ACT_TYPE_BACKUP,
    TLOG_TRX_ACT_TYPE_RESTORE,
    TLOG_TRX_ACT_TYPE_DISCARD
};

/**
 * Transaction data action function prototype.
 *
 * @param level             Transaction nesting level.
 * @param act_type          Action type.
 * @param abstract_object   The abstract object to act on.
 */
typedef void (*tlog_trx_act_fn)(tlog_trx_level level,
                                enum tlog_trx_act_type act_type,
                                void *abstract_object);

#endif /* _TLOG_TRX_ACT_H */
