/*
 * File descriptor JSON log message writer.
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
#include <tlog/rc.h>
#include <tlog/fd_json_writer.h>

/** FD writer data */
struct tlog_fd_json_writer {
    struct tlog_json_writer writer; /**< Abstract writer instance */
    int fd;                         /**< FD to write to */
    bool fd_owned;                  /**< True if FD is owned */
};

static tlog_grc
tlog_fd_json_writer_init(struct tlog_json_writer *writer, va_list ap)
{
    struct tlog_fd_json_writer *fd_json_writer =
                                    (struct tlog_fd_json_writer*)writer;
    fd_json_writer->fd = va_arg(ap, int);
    fd_json_writer->fd_owned = (bool)va_arg(ap, int);
    return TLOG_RC_OK;
}

static void
tlog_fd_json_writer_cleanup(struct tlog_json_writer *writer)
{
    struct tlog_fd_json_writer *fd_json_writer =
                                    (struct tlog_fd_json_writer*)writer;
    if (fd_json_writer->fd_owned) {
        close(fd_json_writer->fd);
        fd_json_writer->fd_owned = false;
    }
}

static tlog_grc
tlog_fd_json_writer_write(struct tlog_json_writer *writer,
                          size_t id, const uint8_t *buf, size_t len)
{
    struct tlog_fd_json_writer *fd_json_writer =
                                    (struct tlog_fd_json_writer*)writer;
    ssize_t rc;
    const uint8_t *p = buf;

    (void)id;

    while (true) {
        rc = write(fd_json_writer->fd, p, len);
        if (rc < 0) {
            /* If interrupted after writing something */
            if (errno == EINTR && p > buf) {
                continue;
            } else {
                return TLOG_GRC_ERRNO;
            }
        }
        if ((size_t)rc == len) {
            return TLOG_RC_OK;
        }
        p += rc;
        len -= (size_t)rc;
    }
}

const struct tlog_json_writer_type tlog_fd_json_writer_type = {
    .size       = sizeof(struct tlog_fd_json_writer),
    .init       = tlog_fd_json_writer_init,
    .write      = tlog_fd_json_writer_write,
    .cleanup    = tlog_fd_json_writer_cleanup,
};
