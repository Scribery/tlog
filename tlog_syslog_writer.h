/*
 * Tlog syslog message writer.
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

#ifndef _TLOG_SYSLOG_WRITER_H
#define _TLOG_SYSLOG_WRITER_H

#include "tlog_writer.h"

/**
 * Syslog message writer type
 *
 * Creation arguments:
 *
 * int  priority    The "priority" argument to pass to syslog(3).
 */
extern const struct tlog_writer_type tlog_syslog_writer_type;

/**
 * Create an instance of syslog writer.
 *
 * @param priority  The "priority" argument to pass to syslog(3).
 *
 * @return The created writer, or NULL in case of error with errno set.
 */
static inline struct tlog_writer*
tlog_syslog_writer_create(int priority)
{
    return tlog_writer_create(&tlog_syslog_writer_type, priority);
}

#endif /* _TLOG_SYSLOG_WRITER_H */
