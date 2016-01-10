/**
 * @file
 * @brief Transaction nesting level.
 *
 * Levels are used to distinguish nested transactions and let each have its
 * own commit/abort boundaries and storage. The top transaction level is
 * TLOG_TRX_LEVEL_MIN and the deepest possible nesting level is
 * TLOG_TRX_LEVEL_MAX.
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

#ifndef _TLOG_TRX_LEVEL_H
#define _TLOG_TRX_LEVEL_H

#include <stdint.h>

/** Transaction nesting level */
typedef unsigned int tlog_trx_level;

/** Minimum transaction nesting level */
#define TLOG_TRX_LEVEL_MIN  0

/** Maximum transaction nesting level */
#define TLOG_TRX_LEVEL_MAX  3

/** Number of available transaction nesting levels */
#define TLOG_TRX_LEVEL_NUM  (TLOG_TRX_LEVEL_MAX + 1)

/** Number of bits required to store a transaction nesting level */
#define TLOG_TRX_LEVEL_BITS 2

#endif /* _TLOG_TRX_LEVEL_H */
