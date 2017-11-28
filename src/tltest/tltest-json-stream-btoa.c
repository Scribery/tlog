/*
 * Tlog tlog_json_stream_btoa function test.
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <tlog/json_stream.h>

int
main(void)
{
    uint8_t i = 0;
    uint8_t btoa_buf[4] = {0,};
    size_t btoa_rc;
    uint8_t snprintf_buf[4] = {0,};
    int snprintf_rc;
    do {
        snprintf_rc = snprintf((char *)snprintf_buf, sizeof(snprintf_buf),
                               "%hhu", i);
        btoa_rc = tlog_json_stream_btoa(btoa_buf, sizeof(btoa_buf), i);
        if (btoa_rc != (size_t)snprintf_rc) {
            fprintf(stderr, "%hhu: rc mismatch: %zu != %d\n",
                    i, btoa_rc, snprintf_rc);
            return 1;
        }
        if (memcmp(btoa_buf, snprintf_buf, 4) != 0) {
            fprintf(stderr,
                    "%hhu: buffer mismatch: "
                    "%02hhx %02hhx %02hhx %02hhx != "
                    "%02hhx %02hhx %02hhx %02hhx\n",
                    i,
                    btoa_buf[0], btoa_buf[1],
                    btoa_buf[2], btoa_buf[3], 
                    snprintf_buf[0], snprintf_buf[1],
                    snprintf_buf[2], snprintf_buf[3]);
            return 1;
        }
        printf("%hhu: %zu == %d, %s == %s\n",
               i, btoa_rc, snprintf_rc, btoa_buf, snprintf_buf);
    } while (i++ != 255);
    return 0;
}
