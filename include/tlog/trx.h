/**
 * @file
 * @brief Transaction support.
 *
 * Transactions are used to keep objects consistent on failures in the middle
 * of operations. So e.g. if a function fails in the middle, the object
 * returns to the previous state.
 *
 * The rule of thumb is: if your function can abort in the middle of
 * processing and doesn't roll back (e.g. it's difficult to implement), then
 * it needs to use transactions.
 *
 * Each such function needs to accept a transaction state argument
 * (tlog_trx_state), define a transaction frame, which is a list of
 * participating objects, begin transaction before processing, commit it on
 * success, or abort on failure. Any transaction-aware functions called from
 * such function need to be passed the transaction state.
 *
 * Transaction state holds the transaction nesting level and transaction stack
 * depth. When transaction is started transaction state stack depth is
 * increased. The transaction data is backed up/restored/discarded only on
 * stack depth zero.
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

#ifndef _TLOG_TRX_H
#define _TLOG_TRX_H

#include <tlog/trx_iface.h>
#include <tlog/trx_state.h>
#include <tlog/trx_frame.h>
#include <tlog/trx_basic.h>

#endif /* _TLOG_TRX_H */
