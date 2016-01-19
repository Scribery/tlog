/*
 * JSON encoder dispatcher interface.
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

#include <tlog/json_dispatcher.h>

static
TLOG_TRX_BASIC_ACT_SIG(tlog_json_dispatcher)
{
    struct tlog_json_dispatcher *dispatcher = \
        (struct tlog_json_dispatcher *)abstract_object;
    dispatcher->container_trx_iface->act(level, act_type,
                                         dispatcher->container);
}

void
tlog_json_dispatcher_init(struct tlog_json_dispatcher *dispatcher,
                          tlog_json_dispatcher_advance_fn advance,
                          tlog_json_dispatcher_reserve_fn reserve,
                          tlog_json_dispatcher_write_fn write,
                          void *container,
                          const struct tlog_trx_iface *container_trx_iface)
{
    assert(dispatcher != NULL);
    assert(advance != NULL);
    assert(reserve != NULL);
    assert(write != NULL);
    assert(container != NULL);
    assert(container_trx_iface != NULL);
    dispatcher->advance = advance;
    dispatcher->reserve = reserve;
    dispatcher->write = write;
    dispatcher->trx_iface = TLOG_TRX_BASIC_IFACE(tlog_json_dispatcher);
    dispatcher->container = container;
    dispatcher->container_trx_iface = container_trx_iface;
    assert(tlog_json_dispatcher_is_valid(dispatcher));
}

