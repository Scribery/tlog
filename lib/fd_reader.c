/*
 * Tlog file descriptor message reader.
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
#include <tlog/fd_reader.h>
#include <tlog/rc.h>

struct tlog_fd_reader {
    struct tlog_reader      reader; /**< Base type */
    struct json_tokener    *tok;    /**< JSON tokener object */
    int                     fd;     /**< Filed descriptor to read from */
    size_t                  line;   /**< Number of the line being read */
    char                   *buf;    /**< Text buffer pointer */
    size_t                  size;   /**< Text buffer size */
    char                   *pos;    /**< Text buffer reading position */
    char                   *end;    /**< End of valid text buffer data */
    bool                    eof;    /**< True if EOF was encountered */
};

static void
tlog_fd_reader_cleanup(struct tlog_reader *reader)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    if (fd_reader->tok != NULL) {
        json_tokener_free(fd_reader->tok);
        fd_reader->tok = NULL;
    }
    free(fd_reader->buf);
    fd_reader->buf = NULL;
}

static tlog_grc
tlog_fd_reader_init(struct tlog_reader *reader, va_list ap)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    int fd = va_arg(ap, int);
    size_t size = va_arg(ap, size_t);
    tlog_grc grc;

    assert(fd >= 0);
    assert(size > 0);

    fd_reader->size = size;

    fd_reader->buf = malloc(size);
    if (fd_reader->buf == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    fd_reader->tok = json_tokener_new();
    if (fd_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    fd_reader->fd = fd;
    fd_reader->line = 1;
    fd_reader->pos = fd_reader->end = fd_reader->buf;

    return TLOG_RC_OK;

error:
    tlog_fd_reader_cleanup(reader);
    return grc;
}

static bool
tlog_fd_reader_is_valid(const struct tlog_reader *reader)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    return fd_reader->tok != NULL &&
           fd_reader->fd >= 0 &&
           fd_reader->line > 0 &&
           fd_reader->size > 0 &&
           fd_reader->end >= fd_reader->buf &&
           fd_reader->pos >= fd_reader->buf &&
           fd_reader->end <= fd_reader->buf + fd_reader->size &&
           fd_reader->pos <= fd_reader->end;
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

/**
 * Refill the buffer of an fd_reader with data from the fd.
 *
 * @param fd_reader     The fd reader to refill the buffer for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_reader_refill_buf(struct tlog_fd_reader *fd_reader)
{
    ssize_t rc;

    assert(tlog_fd_reader_is_valid((struct tlog_reader *)fd_reader));
    assert(fd_reader->pos == fd_reader->end);

    /* Reset buffer */
    fd_reader->pos = fd_reader->end = fd_reader->buf;

    /* Read some more */
    while (!fd_reader->eof &&
           fd_reader->end < (fd_reader->buf + fd_reader->size)) {
        rc = read(fd_reader->fd, fd_reader->end,
                  fd_reader->buf + fd_reader->size -
                    fd_reader->end);
        if (rc == 0) {
            fd_reader->eof = true;
        } else if (rc < 0) {
            if (errno == EINTR)
                continue;
            else
                return TLOG_GRC_ERRNO;
        } else {
            fd_reader->end += rc;
        }
    }

    return TLOG_RC_OK;
}

/**
 * Skip whitespace in the fd reader text.
 *
 * @param fd_reader     The fd reader to skip whitespace for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_reader_skip_whitespace(struct tlog_fd_reader *fd_reader)
{
    tlog_grc grc;

    assert(tlog_fd_reader_is_valid((struct tlog_reader *)fd_reader));

    /* Until EOF */
    do {
        for (; fd_reader->pos < fd_reader->end; fd_reader->pos++) {
            switch (*fd_reader->pos) {
            case '\n':
                fd_reader->line++;
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

        grc = tlog_fd_reader_refill_buf(fd_reader);
        if (grc != TLOG_RC_OK)
            return grc;
    } while (fd_reader->end > fd_reader->buf);

    return TLOG_RC_OK;
}

/**
 * Skip the rest of the line in the fd reader text.
 *
 * @param fd_reader     The fd reader to skip the line for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_reader_skip_line(struct tlog_fd_reader *fd_reader)
{
    tlog_grc grc;

    assert(tlog_fd_reader_is_valid((struct tlog_reader *)fd_reader));

    /* Until EOF */
    do {
        for (; fd_reader->pos < fd_reader->end; fd_reader->pos++) {
            if (*fd_reader->pos == '\n') {
                fd_reader->pos++;
                fd_reader->line++;
                return TLOG_RC_OK;
            }
        }

        grc = tlog_fd_reader_refill_buf(fd_reader);
        if (grc != TLOG_RC_OK)
            return grc;
    } while (fd_reader->end > fd_reader->buf);

    return TLOG_RC_OK;
}

/**
 * Read the fd reader text as (a part of) a JSON object line, don't consume
 * terminating newline.
 *
 * @param fd_reader     The fd reader to parse the object for.
 * @param pobject       Location for the parsed object pointer.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_fd_reader_read_json(struct tlog_fd_reader *fd_reader,
                         struct json_object **pobject)
{
    tlog_grc grc;
    char *p;
    struct json_object *object;
    enum json_tokener_error jerr;
    bool got_text = false;

    assert(tlog_fd_reader_is_valid((struct tlog_reader *)fd_reader));
    assert(pobject != NULL);

    json_tokener_reset(fd_reader->tok);

    /* Until EOF */
    do {
        /* If the buffer is not empty */
        if (fd_reader->pos < fd_reader->end) {
            /* We got something to parse */
            got_text = true;

            /* Look for a terminating newline */
            for (p = fd_reader->pos; p < fd_reader->end && *p != '\n'; p++);

            /* Parse the next piece */
            object = json_tokener_parse_ex(fd_reader->tok,
                                           fd_reader->pos,
                                           p - fd_reader->pos);
            fd_reader->pos = p;

            /* If we finished parsing an object */
            if (object != NULL) {
                *pobject = object;
                return TLOG_RC_OK;
            } else {
                jerr = json_tokener_get_error(fd_reader->tok);
                /* If object is not finished */
                if (jerr == json_tokener_continue) {
                    /* If we encountered an object-terminating newline */
                    if (p < fd_reader->end) {
                        return TLOG_RC_FD_READER_INCOMPLETE_LINE;
                    }
                } else {
                    return TLOG_GRC_FROM(json, jerr);
                }
            }
        }

        grc = tlog_fd_reader_refill_buf(fd_reader);
        if (grc != TLOG_RC_OK)
            return grc;
    } while (fd_reader->end > fd_reader->buf);

    if (got_text) {
        return TLOG_RC_FD_READER_INCOMPLETE_LINE;
    } else {
        *pobject = NULL;
        return TLOG_RC_OK;
    }
}

tlog_grc
tlog_fd_reader_read(struct tlog_reader *reader, struct json_object **pobject)
{
    struct tlog_fd_reader *fd_reader =
                                (struct tlog_fd_reader*)reader;
    tlog_grc grc;
    tlog_grc read_grc;

    /* Skip leading whitespace */
    grc = tlog_fd_reader_skip_whitespace(fd_reader);
    if (grc != TLOG_RC_OK)
        return grc;

    /* (Try to) read the JSON object line */
    read_grc = tlog_fd_reader_read_json(fd_reader, pobject);

    /* Throw away the rest of the line */
    grc = tlog_fd_reader_skip_line(fd_reader);
    if (grc != TLOG_RC_OK)
        return grc;

    return read_grc;
}

const struct tlog_reader_type tlog_fd_reader_type = {
    .size       = sizeof(struct tlog_fd_reader),
    .init       = tlog_fd_reader_init,
    .is_valid   = tlog_fd_reader_is_valid,
    .loc_get    = tlog_fd_reader_loc_get,
    .loc_fmt    = tlog_fd_reader_loc_fmt,
    .read       = tlog_fd_reader_read,
    .cleanup    = tlog_fd_reader_cleanup,
};
