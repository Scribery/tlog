/*
 * Tlog file descriptor message writer.
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

#ifndef _TLOG_FD_WRITER_H
#define _TLOG_FD_WRITER_H

#include <assert.h>
#include <tlog/writer.h>

/**
 * File descriptor message writer type
 *
 * Creation arguments:
 *
 * int  fd      File descriptor to write messages to.
 */
extern const struct tlog_writer_type tlog_fd_writer_type;

/**
 * Create an instance of file descriptor writer.
 *
 * @param pwriter   Location for the created writer pointer, will be set to
 *                  NULL in case of error.
 * @param fd        File descriptor to write messages to.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_fd_writer_create(struct tlog_writer **pwriter, int fd)
{
    assert(fd >= 0);
    return tlog_writer_create(pwriter, &tlog_fd_writer_type, fd);
}

#endif /* _TLOG_FD_WRITER_H */
