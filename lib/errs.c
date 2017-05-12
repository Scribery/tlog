/**
 * Error stack.
 *
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

#include <config.h>
#include <tlog/errs.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const struct tlog_errs tlog_errs_nomem = {
    .next = NULL,
    .msg = "Failed allocating memory for the error stack, messages discarded",
};

void
tlog_errs_destroy(struct tlog_errs **perrs)
{
    assert(perrs != NULL);

    if (*perrs != NULL) {
        if (*perrs != &tlog_errs_nomem) {
            tlog_errs_destroy(&(*perrs)->next);
            free((*perrs)->msg);
            free(*perrs);
        }
        *perrs = NULL;
    }
}

/**
 * Replace a stack with tlog_errs_nomem, indicating memory allocation error.
 *
 * @param perrs Location of the stack to replace.
 */
static void
tlog_errs_replace_with_nomem(struct tlog_errs **perrs)
{
    tlog_errs_destroy(perrs);
    /* We promise to keep it intact */
    *perrs = (struct tlog_errs *)&tlog_errs_nomem;
}

void
tlog_errs_push(struct tlog_errs **perrs, struct tlog_errs *errs)
{
    if (perrs == NULL) {
        tlog_errs_destroy(&errs);
    } else if (errs != NULL) {
        struct tlog_errs *last_errs;

        for (last_errs = errs;
             last_errs->next != NULL;
             last_errs = last_errs->next);

        if (last_errs == &tlog_errs_nomem) {
            tlog_errs_destroy(perrs);
        } else {
            last_errs->next = *perrs;
        }

        *perrs = errs;
    }
}

void
tlog_errs_pushs(struct tlog_errs **perrs, const char *msg)
{
    struct tlog_errs *new_errs;
    char *msg_copy;

    assert(msg != NULL);

    if (perrs != NULL) {
        msg_copy = strdup(msg);
        if (msg_copy == NULL) {
            tlog_errs_replace_with_nomem(perrs);
            return;
        }

        new_errs = malloc(sizeof(*new_errs));
        if (new_errs == NULL) {
            free(msg_copy);
            tlog_errs_replace_with_nomem(perrs);
            return;
        }

        new_errs->next = *perrs;
        new_errs->msg = msg_copy;

        *perrs = new_errs;
    }

    return;
}

void
tlog_errs_pushc(struct tlog_errs **perrs, tlog_grc grc)
{
    assert(tlog_grc_is_valid(grc));

    if (perrs != NULL) {
        tlog_errs_pushs(perrs, tlog_grc_strerror(grc));
    }
}

void
tlog_errs_pushvf(struct tlog_errs **perrs, const char *fmt, va_list ap)
{
    struct tlog_errs *new_errs;
    char *msg = NULL;

    assert(fmt != NULL);

    if (perrs != NULL) {
        if (vasprintf(&msg, fmt, ap) < 0) {
            /* Assume the only error possible is ENOMEM */
            tlog_errs_replace_with_nomem(perrs);
            return;
        }

        new_errs = malloc(sizeof(*new_errs));
        if (new_errs == NULL) {
            free(msg);
            tlog_errs_replace_with_nomem(perrs);
            return;
        }

        new_errs->next = *perrs;
        new_errs->msg = msg;

        *perrs = new_errs;
    }

    return;
}

void
tlog_errs_pushf(struct tlog_errs **perrs, const char *fmt, ...)
{
    va_list ap;

    assert(fmt != NULL);

    if (perrs != NULL) {
        va_start(ap, fmt);
        tlog_errs_pushvf(perrs, fmt, ap);
        va_end(ap);
    }
}

int
tlog_errs_print(FILE *stream, const struct tlog_errs *errs)
{
    int len = 0;
    int rc;

    assert(stream != NULL);

    if (errs != NULL) {
        /* Print lower error */
        rc = tlog_errs_print(stream, errs->next);
        if (rc < 0) {
            return rc;
        }
        len += rc;

        /* Print this error */
        rc = fprintf(stream, "%s\n", errs->msg);
        if (rc < 0) {
            return rc;
        }
        len += rc;
    }

    return len;
}
