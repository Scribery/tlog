/*
 * Syslog JSON message writer.
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

#include <syslog.h>
#include <tlog/rc.h>
#include <tlog/syslog_json_writer.h>

/** Syslog writer data */
struct tlog_syslog_json_writer {
    struct tlog_json_writer writer;     /**< Abstract writer instance */
    int                     priority;   /**< Logging priority */
};

static tlog_grc
tlog_syslog_json_writer_init(struct tlog_json_writer *writer, va_list ap)
{
    struct tlog_syslog_json_writer *syslog_json_writer =
                                    (struct tlog_syslog_json_writer*)writer;
    syslog_json_writer->priority = va_arg(ap, int);
    return TLOG_RC_OK;
}

static tlog_grc
tlog_syslog_json_writer_write(struct tlog_json_writer *writer,
                              size_t id, const uint8_t *buf, size_t len)
{
    struct tlog_syslog_json_writer *syslog_json_writer =
                                    (struct tlog_syslog_json_writer*)writer;
    (void)id;
    syslog(syslog_json_writer->priority, "%.*s",
           (int)len, (const char *)buf);
    return TLOG_RC_OK;
}

const struct tlog_json_writer_type tlog_syslog_json_writer_type = {
    .size   = sizeof(struct tlog_syslog_json_writer),
    .init   = tlog_syslog_json_writer_init,
    .write  = tlog_syslog_json_writer_write,
};
