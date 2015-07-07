/*
 * Tlog test functions.
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

#include "test.h"

static void
tlog_test_diff_side(FILE *stream, const char *name,
                    const uint8_t *out, const uint8_t *exp, size_t len)
{
    size_t col;
    size_t i;
    const uint8_t *o;
    const uint8_t *e;
    uint8_t c;

    fprintf(stream, "%s str:\n", name);
    for (o = out, i = len; i > 0; o++, i--) {
        c = *o;
        fputc(((c >= 0x20 && c < 0x7f) ? c : ' '), stream);
    }
    fputc('\n', stream);
    for (o = out, e = exp, i = len; i > 0; o++, e++, i--)
        fprintf(stream, "%c", ((*o == *e) ? ' ' : '^'));

    fprintf(stream, "\n%s hex:\n", name);
    for (o = out, e = exp, i = len, col = 0; i > 0; o++, e++, i--) {
        fprintf(stream, " %c%02x", ((*o == *e) ? ' ' : '!'), *o);
        col++;
        if (col > 0xf) {
            col = 0;
            fprintf(stream, "\n");
        }
    }
    if (col != 0)
        fprintf(stream, "\n");
}

void
tlog_test_diff(FILE *stream, const uint8_t *res,
               const uint8_t *exp, size_t len)
{
    tlog_test_diff_side(stream, "expected", exp, res, len);
    fprintf(stream, "\n");
    tlog_test_diff_side(stream, "result", res, exp, len);
}


