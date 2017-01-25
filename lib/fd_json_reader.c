/*
 * File descriptor JSON message reader.
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
#include <tlog/fd_json_reader.h>
#include <tlog/rc.h>

/** FD reader data */
struct tlog_fd_json_reader {
    struct tlog_json_reader     reader; /**< Base type */
    struct json_tokener    *tok;        /**< JSON tokener object */
    int                     fd;         /**< Filed descriptor to read from */
    bool                    fd_owned;   /**< True if FD is owned */
    size_t                  line;       /**< Number of the line being read */
    char                   *buf;        /**< Text buffer pointer */
    size_t                  size;       /**< Text buffer size */
    char                   *pos;        /**< Text buffer reading position */
    char                   *end;        /**< End of valid text buffer data */
};

static void
tlog_fd_json_reader_cleanup(struct tlog_json_reader *reader)
{
    struct tlog_fd_json_reader *fd_json_reader =
                                (struct tlog_fd_json_reader*)reader;
    if (fd_json_reader->tok != NULL) {
        json_tokener_free(fd_json_reader->tok);
        fd_json_reader->tok = NULL;
    }
    free(fd_json_reader->buf);
    fd_json_reader->buf = NULL;
    if (fd_json_reader->fd_owned) {
        close(fd_json_reader->fd);
        fd_json_reader->fd_owned = false;
    }
}

static tlog_grc
tlog_fd_json_reader_init(struct tlog_json_reader *reader, va_list ap)
{
    struct tlog_fd_json_reader *fd_json_reader =
                                (struct tlog_fd_json_reader*)reader;
    int fd = va_arg(ap, int);
    bool fd_owned = (bool)va_arg(ap, int);
    size_t size = va_arg(ap, size_t);
    tlog_grc grc;

    assert(fd >= 0);
    assert(size > 0);

    fd_json_reader->size = size;

    fd_json_reader->buf = malloc(size);
    if (fd_json_reader->buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    fd_json_reader->tok = json_tokener_new();
    if (fd_json_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    fd_json_reader->fd = fd;
    fd_json_reader->fd_owned = fd_owned;
    fd_json_reader->line = 1;
    fd_json_reader->pos = fd_json_reader->end = fd_json_reader->buf;

    return TLOG_RC_OK;

error:
    tlog_fd_json_reader_cleanup(reader);
    return grc;
}

static bool
tlog_fd_json_reader_is_valid(const struct tlog_json_reader *reader)
{
    struct tlog_fd_json_reader *fd_json_reader =
                                (struct tlog_fd_json_reader*)reader;
    return fd_json_reader->tok != NULL &&
           fd_json_reader->fd >= 0 &&
           fd_json_reader->line > 0 &&
           fd_json_reader->size > 0 &&
           fd_json_reader->end >= fd_json_reader->buf &&
           fd_json_reader->pos >= fd_json_reader->buf &&
           fd_json_reader->end <=
                fd_json_reader->buf + fd_json_reader->size &&
           fd_json_reader->pos <= fd_json_reader->end;
}

static size_t
tlog_fd_json_reader_loc_get(const struct tlog_json_reader *reader)
{
    return ((struct tlog_fd_json_reader*)reader)->line;
}

static char *
tlog_fd_json_reader_loc_fmt(const struct tlog_json_reader *reader,
                            size_t loc)
{
    char *str;
    (void)reader;
    return asprintf(&str, "line %zu", loc) >= 0 ? str : NULL;
}

/**
 * Refill the buffer of an fd_json_reader with data from the fd.
 *
 * @param fd_json_reader     The fd reader to refill the buffer for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_json_reader_refill_buf(struct tlog_fd_json_reader *fd_json_reader)
{
    ssize_t rc;

    assert(tlog_fd_json_reader_is_valid(
                (struct tlog_json_reader *)fd_json_reader));
    assert(fd_json_reader->pos == fd_json_reader->end);

    /* Reset buffer */
    fd_json_reader->pos = fd_json_reader->end = fd_json_reader->buf;

    /* Read some more */
    while (fd_json_reader->end <
                (fd_json_reader->buf + fd_json_reader->size)) {
        rc = read(fd_json_reader->fd, fd_json_reader->end,
                  fd_json_reader->buf + fd_json_reader->size -
                    fd_json_reader->end);
        if (rc == 0) {
            break;
        } else if (rc < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return TLOG_GRC_ERRNO;
            }
        } else {
            fd_json_reader->end += rc;
        }
    }

    return TLOG_RC_OK;
}

/**
 * Skip whitespace in the fd reader text.
 *
 * @param fd_json_reader     The fd reader to skip whitespace for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_json_reader_skip_whitespace(
                    struct tlog_fd_json_reader *fd_json_reader)
{
    tlog_grc grc;

    assert(tlog_fd_json_reader_is_valid(
                    (struct tlog_json_reader *)fd_json_reader));

    /* Until EOF */
    do {
        for (; fd_json_reader->pos < fd_json_reader->end;
             fd_json_reader->pos++) {
            switch (*fd_json_reader->pos) {
            case '\n':
                fd_json_reader->line++;
            case '\f':
            case '\r':
            case '\t':
            case '\v':
            case ' ':
                break;
            default:
                return TLOG_RC_OK;
            }
        }

        grc = tlog_fd_json_reader_refill_buf(fd_json_reader);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    } while (fd_json_reader->end > fd_json_reader->buf);

    return TLOG_RC_OK;
}

/**
 * Skip the rest of the line in the fd reader text.
 *
 * @param fd_json_reader     The fd reader to skip the line for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_json_reader_skip_line(struct tlog_fd_json_reader *fd_json_reader)
{
    tlog_grc grc;

    assert(tlog_fd_json_reader_is_valid(
                    (struct tlog_json_reader *)fd_json_reader));

    /* Until EOF */
    do {
        for (; fd_json_reader->pos < fd_json_reader->end;
             fd_json_reader->pos++) {
            if (*fd_json_reader->pos == '\n') {
                fd_json_reader->pos++;
                fd_json_reader->line++;
                return TLOG_RC_OK;
            }
        }

        grc = tlog_fd_json_reader_refill_buf(fd_json_reader);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    } while (fd_json_reader->end > fd_json_reader->buf);

    return TLOG_RC_OK;
}

/**
 * Read the fd reader text as (a part of) a JSON object line, don't consume
 * terminating newline.
 *
 * @param fd_json_reader     The fd reader to parse the object for.
 * @param pobject       Location for the parsed object pointer.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_json_reader_read_json(struct tlog_fd_json_reader *fd_json_reader,
                              struct json_object **pobject)
{
    tlog_grc grc;
    char *p;
    struct json_object *object;
    enum json_tokener_error jerr;
    bool got_text = false;

    assert(tlog_fd_json_reader_is_valid(
                    (struct tlog_json_reader *)fd_json_reader));
    assert(pobject != NULL);

    json_tokener_reset(fd_json_reader->tok);

    /* Until EOF */
    do {
        /* If the buffer is not empty */
        if (fd_json_reader->pos < fd_json_reader->end) {
            /* We got something to parse */
            got_text = true;

            /* Look for a terminating newline */
            for (p = fd_json_reader->pos;
                 p < fd_json_reader->end && *p != '\n';
                 p++);

            /* Parse the next piece */
            object = json_tokener_parse_ex(fd_json_reader->tok,
                                           fd_json_reader->pos,
                                           p - fd_json_reader->pos);
            fd_json_reader->pos = p;

            /* If we finished parsing an object */
            if (object != NULL) {
                *pobject = object;
                return TLOG_RC_OK;
            } else {
                jerr = json_tokener_get_error(fd_json_reader->tok);
                /* If object is not finished */
                if (jerr == json_tokener_continue) {
                    /* If we encountered an object-terminating newline */
                    if (p < fd_json_reader->end) {
                        return TLOG_RC_FD_JSON_READER_INCOMPLETE_LINE;
                    }
                } else {
                    return TLOG_GRC_FROM(json, jerr);
                }
            }
        }

        grc = tlog_fd_json_reader_refill_buf(fd_json_reader);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    } while (fd_json_reader->end > fd_json_reader->buf);

    if (got_text) {
        return TLOG_RC_FD_JSON_READER_INCOMPLETE_LINE;
    } else {
        *pobject = NULL;
        return TLOG_RC_OK;
    }
}

tlog_grc
tlog_fd_json_reader_read(struct tlog_json_reader *reader,
                         struct json_object **pobject)
{
    struct tlog_fd_json_reader *fd_json_reader =
                                (struct tlog_fd_json_reader*)reader;
    tlog_grc grc;
    tlog_grc read_grc;

    /* Skip leading whitespace */
    grc = tlog_fd_json_reader_skip_whitespace(fd_json_reader);
    if (grc != TLOG_RC_OK) {
        return grc;
    }

    /* (Try to) read the JSON object line */
    read_grc = tlog_fd_json_reader_read_json(fd_json_reader, pobject);

    /* Throw away the rest of the line */
    grc = tlog_fd_json_reader_skip_line(fd_json_reader);
    if (grc != TLOG_RC_OK) {
        return grc;
    }

    return read_grc;
}

const struct tlog_json_reader_type tlog_fd_json_reader_type = {
    .size       = sizeof(struct tlog_fd_json_reader),
    .init       = tlog_fd_json_reader_init,
    .is_valid   = tlog_fd_json_reader_is_valid,
    .loc_get    = tlog_fd_json_reader_loc_get,
    .loc_fmt    = tlog_fd_json_reader_loc_fmt,
    .read       = tlog_fd_json_reader_read,
    .cleanup    = tlog_fd_json_reader_cleanup,
};
