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

#include <unistd.h>
#include <errno.h>
#include "tlog/rc.h"
#include "tlog/fd_writer.h"

struct tlog_fd_writer {
    struct tlog_writer writer;
    int fd;
};

static tlog_grc
tlog_fd_writer_init(struct tlog_writer *writer, va_list ap)
{
    struct tlog_fd_writer *fd_writer =
                                (struct tlog_fd_writer*)writer;
    fd_writer->fd = va_arg(ap, int);
    return TLOG_RC_OK;
}

tlog_grc
tlog_fd_writer_write(struct tlog_writer *writer,
                         const uint8_t *buf,
                         size_t len)
{
    struct tlog_fd_writer *fd_writer =
                                (struct tlog_fd_writer*)writer;
    ssize_t rc;

    while (true) {
        rc = write(fd_writer->fd, buf, len);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            else
                return TLOG_GRC_FROM(errno, errno);
        }
        if ((size_t)rc == len)
            return TLOG_RC_OK;
        buf += rc;
        len -= (size_t)rc;
    }
}

const struct tlog_writer_type tlog_fd_writer_type = {
    .size   = sizeof(struct tlog_fd_writer),
    .init   = tlog_fd_writer_init,
    .write  = tlog_fd_writer_write,
};
