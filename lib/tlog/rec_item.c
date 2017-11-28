/**
 * Recorded item.
 *
 * Copyright (C) 2016-2017 Red Hat
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

#include <tlog/rec_item.h>

enum tlog_rec_item
tlog_rec_item_from_pkt(const struct tlog_pkt *pkt)
{
    assert(tlog_pkt_is_valid(pkt));
    assert(!tlog_pkt_is_void(pkt));
    return pkt->type == TLOG_PKT_TYPE_WINDOW
                ? TLOG_REC_ITEM_WINDOW
                : (pkt->data.io.output
                        ? TLOG_REC_ITEM_OUTPUT
                        : TLOG_REC_ITEM_INPUT);
}
