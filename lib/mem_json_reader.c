/*
 * JSON memory buffer message reader.
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
#include <tlog/mem_json_reader.h>
#include <tlog/rc.h>

/** Memory buffer reader data */
struct tlog_mem_json_reader {
    struct tlog_json_reader reader; /**< Base type */
    struct json_tokener    *tok;    /**< JSON tokener object */
    size_t                  line;   /**< Number of the line being read */
    const char             *pos;    /**< Buffer reading position */
    const char             *end;    /**< End of buffer */
};

static void
tlog_mem_json_reader_cleanup(struct tlog_json_reader *reader)
{
    struct tlog_mem_json_reader *mem_json_reader =
                                (struct tlog_mem_json_reader*)reader;
    if (mem_json_reader->tok != NULL) {
        json_tokener_free(mem_json_reader->tok);
        mem_json_reader->tok = NULL;
    }
}

static tlog_grc
tlog_mem_json_reader_init(struct tlog_json_reader *reader, va_list ap)
{
    struct tlog_mem_json_reader *mem_json_reader =
                                (struct tlog_mem_json_reader*)reader;
    const char *buf = va_arg(ap, const char *);
    size_t len = va_arg(ap, size_t);
    tlog_grc grc;

    assert(buf != NULL || len == 0);

    mem_json_reader->pos = buf;
    mem_json_reader->end = buf + len;
    mem_json_reader->line = 1;

    mem_json_reader->tok = json_tokener_new();
    if (mem_json_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    return TLOG_RC_OK;

error:
    tlog_mem_json_reader_cleanup(reader);
    return grc;
}

static bool
tlog_mem_json_reader_is_valid(const struct tlog_json_reader *reader)
{
    struct tlog_mem_json_reader *mem_json_reader =
                                (struct tlog_mem_json_reader*)reader;
    return mem_json_reader->tok != NULL &&
           mem_json_reader->line > 0 &&
           mem_json_reader->pos <= mem_json_reader->end;
}

static size_t
tlog_mem_json_reader_loc_get(const struct tlog_json_reader *reader)
{
    return ((struct tlog_mem_json_reader*)reader)->line;
}

static char *
tlog_mem_json_reader_loc_fmt(const struct tlog_json_reader *reader,
                             size_t loc)
{
    char *str;
    (void)reader;
    return asprintf(&str, "line %zu", loc) >= 0 ? str : NULL;
}

/**
 * Skip whitespace in the memory reader text.
 *
 * @param mem_json_reader    The memory reader to skip whitespace for.
 */
static void
tlog_mem_json_reader_skip_whitespace(
                struct tlog_mem_json_reader *mem_json_reader)
{
    assert(tlog_mem_json_reader_is_valid(
                    (struct tlog_json_reader *)mem_json_reader));

    for (; mem_json_reader->pos < mem_json_reader->end;
         mem_json_reader->pos++) {
        switch (*mem_json_reader->pos) {
        case '\n':
            mem_json_reader->line++;
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
 * @param mem_json_reader    The memory reader to skip the line for.
 */
static void
tlog_mem_json_reader_skip_line(struct tlog_mem_json_reader *mem_json_reader)
{
    assert(tlog_mem_json_reader_is_valid(
                    (struct tlog_json_reader *)mem_json_reader));

    for (; mem_json_reader->pos < mem_json_reader->end;
         mem_json_reader->pos++) {
        if (*mem_json_reader->pos == '\n') {
            mem_json_reader->pos++;
            mem_json_reader->line++;
            break;
        }
    }
}

/**
 * Read the memory reader text as a JSON object line, don't consume
 * terminating newline. The line must not start with whitespace.
 *
 * @param mem_json_reader    The memory reader to parse the object for.
 * @param pobject       Location for the parsed object pointer.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_mem_json_reader_read_json(struct tlog_mem_json_reader *mem_json_reader,
                               struct json_object **pobject)
{
    const char *p;
    struct json_object *object;
    enum json_tokener_error jerr;

    assert(tlog_mem_json_reader_is_valid(
                    (struct tlog_json_reader *)mem_json_reader));
    assert(pobject != NULL);

    json_tokener_reset(mem_json_reader->tok);

    /* Look for a terminating newline */
    for (p = mem_json_reader->pos;
         p < mem_json_reader->end && *p != '\n';
         p++);

    /* If the line is empty */
    if (p == mem_json_reader->pos) {
        /* Report EOF */
        *pobject = NULL;
        return TLOG_RC_OK;
    }

    /* Parse the line */
    object = json_tokener_parse_ex(mem_json_reader->tok,
                                   mem_json_reader->pos,
                                   p - mem_json_reader->pos);
    mem_json_reader->pos = p;
    if (object == NULL) {
        jerr = json_tokener_get_error(mem_json_reader->tok);
        return (jerr == json_tokener_continue)
                    ? TLOG_RC_MEM_JSON_READER_INCOMPLETE_LINE
                    : TLOG_GRC_FROM(json, jerr);
    }

    /* Return the parsed object */
    *pobject = object;
    return TLOG_RC_OK;
}

tlog_grc
tlog_mem_json_reader_read(struct tlog_json_reader *reader,
                          struct json_object **pobject)
{
    struct tlog_mem_json_reader *mem_json_reader =
                                (struct tlog_mem_json_reader*)reader;
    tlog_grc grc;

    /* Skip leading whitespace */
    tlog_mem_json_reader_skip_whitespace(mem_json_reader);

    /* (Try to) read the JSON object line */
    grc = tlog_mem_json_reader_read_json(mem_json_reader, pobject);

    /* Throw away the rest of the line */
    tlog_mem_json_reader_skip_line(mem_json_reader);

    return grc;
}

const struct tlog_json_reader_type tlog_mem_json_reader_type = {
    .size       = sizeof(struct tlog_mem_json_reader),
    .init       = tlog_mem_json_reader_init,
    .is_valid   = tlog_mem_json_reader_is_valid,
    .loc_get    = tlog_mem_json_reader_loc_get,
    .loc_fmt    = tlog_mem_json_reader_loc_fmt,
    .read       = tlog_mem_json_reader_read,
    .cleanup    = tlog_mem_json_reader_cleanup,
};
