/*
 * Rate-limiting JSON message writer.
 *
 * Copyright (C) 2017 Red Hat
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

#include <tlog/timespec.h>
#include <tlog/rc.h>
#include <tlog/rl_json_writer.h>

/** Rate-limiting writer data */
struct tlog_rl_json_writer {
    /** Abstract writer instance */
    struct tlog_json_writer     writer;
    /** "Below" writer the packets should be written to */
    struct tlog_json_writer    *below;
    /** True if "below" writer should be destroyed with us */
    bool                        below_owned;
    /** ID of the clock to use for calculating rate and delaying */
    clockid_t                   clock_id;
    /**
     * Average message rate limit, bytes per second.
     * Type is chosen to be compatible with timestamps.
     */
    struct timespec             rate;
    /**
     * Maximum message burst size, bytes.
     * Type is chosen to be compatible with timestamps.
     */
    struct timespec             burst;
    /**
     * Bucket limit (rate + burst), bytes.
     * Type is chosen to be compatible with timestamps.
     */
    struct timespec             limit;
    /** True if messages exceeding rate should be dropped, false if delayed */
    bool                        drop;
    /** True if the writer synced time and bucket previously */
    bool                        synced;
    /** Last sync timestamp */
    struct timespec             last_sync;
    /**
     * "Leaky bucket" of message sizes.
     * Type is chosen to be compatible with timestamps.
     */
    struct timespec             bucket;
};

static tlog_grc
tlog_rl_json_writer_init(struct tlog_json_writer *writer, va_list ap)
{
    struct tlog_rl_json_writer *rl_json_writer =
                                    (struct tlog_rl_json_writer*)writer;
    rl_json_writer->below = va_arg(ap, struct tlog_json_writer *);
    assert(tlog_json_writer_is_valid(rl_json_writer->below));
    rl_json_writer->below_owned = va_arg(ap, int) != 0;
    rl_json_writer->clock_id = va_arg(ap, clockid_t);
    rl_json_writer->rate.tv_sec = (time_t)va_arg(ap, size_t);
    rl_json_writer->burst.tv_sec = (time_t)va_arg(ap, size_t);
    tlog_timespec_add(&rl_json_writer->rate, &rl_json_writer->burst,
                      &rl_json_writer->limit);
    rl_json_writer->drop = va_arg(ap, int) != 0;
    return TLOG_RC_OK;
}

static bool
tlog_rl_json_writer_is_valid(const struct tlog_json_writer *writer)
{
    struct tlog_rl_json_writer *rl_json_writer =
                                    (struct tlog_rl_json_writer*)writer;
    return rl_json_writer != NULL &&
           tlog_json_writer_is_valid(rl_json_writer->below) &&
           tlog_timespec_is_valid(&rl_json_writer->rate) &&
           tlog_timespec_is_valid(&rl_json_writer->burst) &&
           (!rl_json_writer->synced ||
            tlog_timespec_is_valid(&rl_json_writer->last_sync)) &&
           tlog_timespec_cmp(&rl_json_writer->bucket,
                             &rl_json_writer->limit) <= 0;
}

static void
tlog_rl_json_writer_cleanup(struct tlog_json_writer *writer)
{
    struct tlog_rl_json_writer *rl_json_writer =
                                    (struct tlog_rl_json_writer*)writer;
    assert(rl_json_writer != NULL);
    if (rl_json_writer->below_owned) {
        tlog_json_writer_destroy(rl_json_writer->below);
    }
    rl_json_writer->below = NULL;
}

static tlog_grc
tlog_rl_json_writer_write(struct tlog_json_writer *writer,
                           size_t id, const uint8_t *buf, size_t len)
{
    struct tlog_rl_json_writer *rl_json_writer =
                                    (struct tlog_rl_json_writer*)writer;
    tlog_grc grc;
    int rc;
    struct timespec now;
    struct timespec poured = {.tv_sec = len, .tv_nsec = 0};
    struct timespec bucket_poured;
    struct timespec overflow;

    /*
     * Sync (drain) the bucket to the time
     */
    if (clock_gettime(rl_json_writer->clock_id, &now) < 0) {
        return TLOG_GRC_ERRNO;
    }

    if (rl_json_writer->synced) {
        struct timespec elapsed;
        struct timespec drained;
        tlog_timespec_sub(&now, &rl_json_writer->last_sync, &elapsed);
        tlog_timespec_fp_mul(&elapsed, &rl_json_writer->rate, &drained);
        tlog_timespec_sub(&rl_json_writer->bucket, &drained,
                          &rl_json_writer->bucket);
        if (tlog_timespec_is_negative(&rl_json_writer->bucket)) {
            rl_json_writer->bucket = TLOG_TIMESPEC_ZERO;
        }
    } else {
        rl_json_writer->synced = true;
    }
    rl_json_writer->last_sync = now;

    /*
     * Try to fit the message into the bucket
     */
    tlog_timespec_add(&rl_json_writer->bucket, &poured, &bucket_poured);
    tlog_timespec_sub(&bucket_poured, &rl_json_writer->limit, &overflow);
    /* If the bucket would overflow */
    if (tlog_timespec_is_positive(&overflow)) {
        /* If dropping */
        if (rl_json_writer->drop) {
            /* Report success without writing */
            return TLOG_RC_OK;
        } else {
            struct timespec delay;
            struct timespec wakeup;
            /* Wait until the message fits */
            tlog_timespec_fp_div(&overflow, &rl_json_writer->rate, &delay);
            tlog_timespec_add(&rl_json_writer->last_sync, &delay, &wakeup);
            rc = clock_nanosleep(rl_json_writer->clock_id, TIMER_ABSTIME,
                                 &wakeup, NULL);
            if (rc != 0) {
                return TLOG_GRC_FROM(errno, rc);
            }
            bucket_poured = rl_json_writer->limit;
            now = wakeup;
        }
    }

    /*
     * Write the message and pour it into bucket
     */
    grc = tlog_json_writer_write(rl_json_writer->below, id, buf, len);
    if (grc != TLOG_RC_OK) {
        return grc;
    }
    rl_json_writer->bucket = bucket_poured;
    rl_json_writer->last_sync = now;

    return TLOG_RC_OK;
}

const struct tlog_json_writer_type tlog_rl_json_writer_type = {
    .size       = sizeof(struct tlog_rl_json_writer),
    .init       = tlog_rl_json_writer_init,
    .is_valid   = tlog_rl_json_writer_is_valid,
    .cleanup    = tlog_rl_json_writer_cleanup,
    .write      = tlog_rl_json_writer_write,
};
