/*
 * Tlog syslog message reader.
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

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <json_tokener.h>
#include "tlog/fd_reader.h"

#define TLOG_FD_READER_BUF_SIZE 32

struct tlog_fd_reader {
    struct tlog_reader reader;
    struct json_tokener *tok;
    int fd;
    size_t line;
    char buf[TLOG_FD_READER_BUF_SIZE];
    char *pos;
    char *end;
};

static char *
tlog_fd_reader_strerror(int error)
{
    char *str = NULL;
    assert(error > 0);

    if (error < TLOG_FD_READER_ERROR_MIN) {
        return strdup(json_tokener_error_desc(error));
    } else {
        switch (error) {
            case TLOG_FD_READER_ERROR_INCOMPLETE_LINE:
                return strdup("Incomplete message object line encountered");
            default:
                asprintf(&str, "Unknown error code %d", error);
                return str;
        }
    }
}

static int
tlog_fd_reader_init(struct tlog_reader *reader, va_list ap)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    int fd = va_arg(ap, int);
    assert(fd >= 0);
    fd_reader->tok = json_tokener_new_ex(2);
    if (fd_reader->tok == NULL)
        return -errno;
    fd_reader->fd = fd;
    fd_reader->line = 1;
    fd_reader->pos = fd_reader->end = fd_reader->buf;

    return 0;
}

static bool
tlog_fd_reader_is_valid(const struct tlog_reader *reader)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    return fd_reader->tok != NULL &&
           fd_reader->fd >= 0 &&
           fd_reader->line > 0 &&
           fd_reader->end >= fd_reader->buf &&
           fd_reader->pos >= fd_reader->buf &&
           fd_reader->end <= fd_reader->buf + sizeof(fd_reader->buf) &&
           fd_reader->pos <= fd_reader->end;
}

static void
tlog_fd_reader_cleanup(struct tlog_reader *reader)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    json_tokener_free(fd_reader->tok);
    fd_reader->tok = NULL;
}

static size_t
tlog_fd_reader_loc_get(const struct tlog_reader *reader)
{
    return ((struct tlog_fd_reader*)reader)->line;
}

static char *
tlog_fd_reader_loc_fmt(size_t loc)
{
    char *str = NULL;
    asprintf(&str, "line %zu", loc);
    return str;
}

int
tlog_fd_reader_read(struct tlog_reader *reader, struct json_object **pobject)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    char *p;
    char c;
    bool got_text = false;
    struct json_object *object;
    enum json_tokener_error jerr;
    ssize_t rc;

    json_tokener_reset(fd_reader->tok);

    do {
        /* If the buffer is not empty */
        while (fd_reader->pos < fd_reader->end) {
            /* Look for a terminating newline, noticing text presence */
            for (p = fd_reader->pos; p < fd_reader->end; p++) {
                c = *p;
                if (c == '\n') {
                    break;
                } else switch (c) {
                    case '\f':
                    case '\r':
                    case '\t':
                    case '\v':
                    case ' ':
                        break;
                    default:
                        got_text = true;
                        break;
                }
            }

            /* Parse next piece if we got text */
            if (got_text) {
                object = json_tokener_parse_ex(fd_reader->tok,
                                               fd_reader->pos,
                                               p - fd_reader->pos);
            }

            fd_reader->pos = p;
            /* If we hit a newline */
            if (p < fd_reader->end) {
                fd_reader->line++;
                fd_reader->pos++;
            }

            /* If we started parsing */
            if (got_text) {
                /* If we finished parsing an object */
                if (object != NULL) {
                    *pobject = object;
                    return 0;
                } else {
                    jerr = json_tokener_get_error(fd_reader->tok);
                    /* If object is not finished */
                    if (jerr == json_tokener_continue) {
                        /* If we encountered an object-terminating newline */
                        if (p < fd_reader->end) {
                            return TLOG_FD_READER_ERROR_INCOMPLETE_LINE;
                        }
                    } else {
                        return jerr;
                    }
                }
            }
        }

        /* Reset buffer */
        fd_reader->pos = fd_reader->end = fd_reader->buf;

        /* Read some more */
        do {
            rc = read(fd_reader->fd, fd_reader->end,
                      fd_reader->buf + sizeof(fd_reader->buf) -
                        fd_reader->end);
            if (rc < 0) {
                if (errno == EINTR)
                    continue;
                else
                    return -errno;
            }
            fd_reader->end += rc;
        } while (rc != 0 &&
                 fd_reader->end < (fd_reader->buf + sizeof(fd_reader->buf)));
    } while (fd_reader->end > fd_reader->buf);

    if (got_text) {
        return TLOG_FD_READER_ERROR_INCOMPLETE_LINE;
    } else {
        *pobject = NULL;
        return 0;
    }
}

const struct tlog_reader_type tlog_fd_reader_type = {
    .size       = sizeof(struct tlog_fd_reader),
    .strerror   = tlog_fd_reader_strerror,
    .init       = tlog_fd_reader_init,
    .is_valid   = tlog_fd_reader_is_valid,
    .loc_get    = tlog_fd_reader_loc_get,
    .loc_fmt    = tlog_fd_reader_loc_fmt,
    .read       = tlog_fd_reader_read,
    .cleanup    = tlog_fd_reader_cleanup,
};
