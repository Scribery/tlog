/*
 * Tlog memory buffer message reader.
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
#include <json_tokener.h>
#include <tlog/mem_reader.h>
#include <tlog/rc.h>

struct tlog_mem_reader {
    struct tlog_reader      reader; /**< Base type */
    struct json_tokener    *tok;    /**< JSON tokener object */
    size_t                  line;   /**< Number of the line being read */
    const char             *pos;    /**< Buffer reading position */
    const char             *end;    /**< End of buffer */
};

static void
tlog_mem_reader_cleanup(struct tlog_reader *reader)
{
    struct tlog_mem_reader *mem_reader =
                                (struct tlog_mem_reader*)reader;
    if (mem_reader->tok != NULL) {
        json_tokener_free(mem_reader->tok);
        mem_reader->tok = NULL;
    }
}

static tlog_grc
tlog_mem_reader_init(struct tlog_reader *reader, va_list ap)
{
    struct tlog_mem_reader *mem_reader =
                                (struct tlog_mem_reader*)reader;
    const char *buf = va_arg(ap, const char *);
    size_t len = va_arg(ap, size_t);
    tlog_grc grc;

    assert(buf != NULL || len == 0);

    mem_reader->pos = buf;
    mem_reader->end = buf + len;
    mem_reader->line = 1;

    mem_reader->tok = json_tokener_new();
    if (mem_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    return TLOG_RC_OK;

error:
    tlog_mem_reader_cleanup(reader);
    return grc;
}

static bool
tlog_mem_reader_is_valid(const struct tlog_reader *reader)
{
    struct tlog_mem_reader *mem_reader =
                                (struct tlog_mem_reader*)reader;
    return mem_reader->tok != NULL &&
           mem_reader->line > 0 &&
           mem_reader->pos <= mem_reader->end;
}

static size_t
tlog_mem_reader_loc_get(const struct tlog_reader *reader)
{
    return ((struct tlog_mem_reader*)reader)->line;
}

static char *
tlog_mem_reader_loc_fmt(size_t loc)
{
    char *str = NULL;
    asprintf(&str, "line %zu", loc);
    return str;
}

/**
 * Skip whitespace in the memory reader text.
 *
 * @param mem_reader    The memory reader to skip whitespace for.
 */
static void
tlog_mem_reader_skip_whitespace(struct tlog_mem_reader *mem_reader)
{
    assert(tlog_mem_reader_is_valid((struct tlog_reader *)mem_reader));

    for (; mem_reader->pos < mem_reader->end; mem_reader->pos++) {
        switch (*mem_reader->pos) {
        case '\n':
            mem_reader->line++;
        case '\f':
        case '\r':
        case '\t':
        case '\v':
        case ' ':
            break;
        default:
            return;
        }
    }
}

/**
 * Skip the rest of the line in the memory reader text.
 *
 * @param mem_reader    The memory reader to skip the line for.
 */
static void
tlog_mem_reader_skip_line(struct tlog_mem_reader *mem_reader)
{
    assert(tlog_mem_reader_is_valid((struct tlog_reader *)mem_reader));

    for (; mem_reader->pos < mem_reader->end; mem_reader->pos++) {
        if (*mem_reader->pos == '\n') {
            mem_reader->pos++;
            mem_reader->line++;
            break;
        }
    }
}

/**
 * Read the memory reader text as a JSON object line, don't consume
 * terminating newline. The line must not start with whitespace.
 *
 * @param mem_reader    The memory reader to parse the object for.
 * @param pobject       Location for the parsed object pointer.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_mem_reader_read_json(struct tlog_mem_reader *mem_reader,
                         struct json_object **pobject)
{
    const char *p;
    struct json_object *object;
    enum json_tokener_error jerr;

    assert(tlog_mem_reader_is_valid((struct tlog_reader *)mem_reader));
    assert(pobject != NULL);

    json_tokener_reset(mem_reader->tok);

    /* Look for a terminating newline */
    for (p = mem_reader->pos; p < mem_reader->end && *p != '\n'; p++);

    /* If the line is empty */
    if (p == mem_reader->pos) {
        /* Report EOF */
        *pobject = NULL;
        return TLOG_RC_OK;
    }

    /* Parse the line */
    object = json_tokener_parse_ex(mem_reader->tok,
                                   mem_reader->pos,
                                   p - mem_reader->pos);
    mem_reader->pos = p;
    if (object == NULL) {
        jerr = json_tokener_get_error(mem_reader->tok);
        return (jerr == json_tokener_continue)
                    ? TLOG_RC_MEM_READER_INCOMPLETE_LINE
                    : TLOG_GRC_FROM(json, jerr);
    }

    /* Return the parsed object */
    *pobject = object;
    return TLOG_RC_OK;
}

tlog_grc
tlog_mem_reader_read(struct tlog_reader *reader, struct json_object **pobject)
{
    struct tlog_mem_reader *mem_reader =
                                (struct tlog_mem_reader*)reader;
    tlog_grc grc;

    /* Skip leading whitespace */
    tlog_mem_reader_skip_whitespace(mem_reader);

    /* (Try to) read the JSON object line */
    grc = tlog_mem_reader_read_json(mem_reader, pobject);

    /* Throw away the rest of the line */
    tlog_mem_reader_skip_line(mem_reader);

    return grc;
}

const struct tlog_reader_type tlog_mem_reader_type = {
    .size       = sizeof(struct tlog_mem_reader),
    .init       = tlog_mem_reader_init,
    .is_valid   = tlog_mem_reader_is_valid,
    .loc_get    = tlog_mem_reader_loc_get,
    .loc_fmt    = tlog_mem_reader_loc_fmt,
    .read       = tlog_mem_reader_read,
    .cleanup    = tlog_mem_reader_cleanup,
};
