/*
 * Miscellaneous test functions.
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

#include <tlog/misc.h>
#include <tltest/misc.h>

static void
tltest_diff_side(FILE *stream, const char *name,
                 const uint8_t *out_buf, size_t out_len,
                 const uint8_t *exp_buf, size_t exp_len)
{
    size_t min_len = TLOG_MIN(out_len, exp_len);
    size_t max_len = TLOG_MAX(out_len, exp_len);
    size_t col;
    size_t i;
    const uint8_t *o;
    const uint8_t *e;
    uint8_t c;

    fprintf(stream, "%s str:\n", name);
    for (o = out_buf, i = 0; i < out_len; o++, i++) {
        c = *o;
        fputc(((c >= 0x20 && c < 0x7f) ? c : ' '), stream);
    }
    fputc('\n', stream);
    for (o = out_buf, e = exp_buf, i = 0; i < max_len; o++, e++, i++) {
        fprintf(stream, "%c",
                ((i < min_len && *o == *e) ? ' ' : '^'));
    }

    fprintf(stream, "\n%s hex:\n", name);
    for (o = out_buf, e = exp_buf, i = 0, col = 0;
         i < max_len;
         o++, e++, i++) {
        fprintf(stream, " %c%c%c",
                ((i < min_len && *o == *e) ? ' ' : '!'),
                (i < out_len ? tlog_nibble_digit(*o >> 4) : ' '),
                (i < out_len ? tlog_nibble_digit(*o & 0xf) : ' '));
        col++;
        if (col > 0xf) {
            col = 0;
            fprintf(stream, "\n");
        }
    }
    if (col != 0) {
        fprintf(stream, "\n");
    }
}

void
tltest_diff(FILE *stream,
            const uint8_t *res_buf, size_t res_len,
            const uint8_t *exp_buf, size_t exp_len)
{
    tltest_diff_side(stream, "expected", exp_buf, exp_len, res_buf, res_len);
    fprintf(stream, "\n");
    tltest_diff_side(stream, "result", res_buf, res_len, exp_buf, exp_len);
}

