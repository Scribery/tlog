/*
 * UTF-8 byte sequence validation state.
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

#include <tlog/utf8.h>

/** Source: Unicode 7.0.0 Chapter 3, Table 3-7 */
const struct tlog_utf8_seq tlog_utf8_seq_list[] = {
    {{{0x00, 0x7f}}},
    {{{0xc2, 0xdf},     {0x80, 0xbf}}},
    {{{0xe0, 0xe0},     {0xa0, 0xbf},   {0x80, 0xbf}}},
    {{{0xe1, 0xec},     {0x80, 0xbf},   {0x80, 0xbf}}},
    {{{0xed, 0xed},     {0x80, 0x9f},   {0x80, 0xbf}}},
    {{{0xee, 0xef},     {0x80, 0xbf},   {0x80, 0xbf}}},
    {{{0xf0, 0xf0},     {0x90, 0xbf},   {0x80, 0xbf},   {0x80, 0xbf}}},
    {{{0xf1, 0xf3},     {0x80, 0xbf},   {0x80, 0xbf},   {0x80, 0xbf}}},
    {{{0xf4, 0xf4},     {0x80, 0x8f},   {0x80, 0xbf},   {0x80, 0xbf}}},
    {{}}
};

bool
tlog_utf8_buf_is_valid(const char *ptr, size_t len)
{
    struct tlog_utf8 utf8 = TLOG_UTF8_INIT;

    assert(ptr != NULL || len == 0);

    for (; len > 0; len--, ptr++) {
        if (!tlog_utf8_add(&utf8, *(const uint8_t *)ptr)) {
            return false;
        }
        if (tlog_utf8_is_ended(&utf8)) {
            tlog_utf8_reset(&utf8);
        }
    }

    return !tlog_utf8_is_started(&utf8);
}
