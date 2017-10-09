/**
 * @file
 * @brief TTY terminal data source.
 *
 * TTY terminal data source provides an interface to read terminal data
 * packets from two file descriptors (bound to TTYs or files), optionally
 * aided by a SIGWINCH handler.
 */
/*
 * Copyright (C) 2016 Red Hat
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

#ifndef _TLOG_TTY_SOURCE_H
#define _TLOG_TTY_SOURCE_H

#include <tlog/source.h>

/** Minimum size of the I/O buffer */
#define TLOG_TTY_SOURCE_IO_SIZE_MIN 32

/** TTY source type */
extern const struct tlog_source_type tlog_tty_source_type;

/**
 * Create (allocate and initialize) a TTY source.
 *
 * @param psource   Location for created source pointer, set to NULL
 *                  in case of error.
 * @param in_fd     File descriptor to read terminal input from,
 *                  or negative number if that's not needed.
 * @param out_fd    File descriptor to read terminal output from,
 *                  or negative number if that's not needed.
 * @param win_fd    File descriptor to read window sizes from,
 *                  or negative number if that's not needed.
 * @param io_size   Size of I/O data buffer used in packets.
 * @param clock_id  Clock to use for timestamps.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_tty_source_create(struct tlog_source **psource,
                       int in_fd, int out_fd, int win_fd,
                       size_t io_size, clockid_t clock_id)
{
    assert(psource != NULL);
    assert(io_size >= TLOG_TTY_SOURCE_IO_SIZE_MIN);
    return tlog_source_create(psource, &tlog_tty_source_type,
                              in_fd, out_fd, win_fd, io_size,
                              clock_id);
}

#endif /* _TLOG_TTY_SOURCE_H */
