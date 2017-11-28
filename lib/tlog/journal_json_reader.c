/*
 * Systemd journal JSON message reader
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

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <json_tokener.h>
#include <tlog/journal_json_reader.h>
#include <tlog/rc.h>

/** FD reader data */
struct tlog_journal_json_reader {
    struct tlog_json_reader     reader;     /**< Base type */
    struct json_tokener        *tok;        /**< JSON tokener object */
    sd_journal                 *journal;    /**< Journal context */
    uint64_t                    until;      /**< Timestamp to stop at,
                                                 milliseconds */
    uint64_t                    last;       /**< Last retrieved timestamp,
                                                 milliseconds */
    size_t                      entry;      /**< Sequential number of the
                                                 entry to be read next,
                                                 starting with zero */
};

static void
tlog_journal_json_reader_cleanup(struct tlog_json_reader *reader)
{
    struct tlog_journal_json_reader *journal_json_reader =
                                (struct tlog_journal_json_reader*)reader;
    if (journal_json_reader->tok != NULL) {
        json_tokener_free(journal_json_reader->tok);
        journal_json_reader->tok = NULL;
    }
    if (journal_json_reader->journal != NULL) {
        sd_journal_close(journal_json_reader->journal);
        journal_json_reader->journal = NULL;
    }
}

static tlog_grc
tlog_journal_json_reader_init(struct tlog_json_reader *reader, va_list ap)
{
    struct tlog_journal_json_reader *journal_json_reader =
                                (struct tlog_journal_json_reader*)reader;
    uint64_t since = va_arg(ap, uint64_t);
    uint64_t until = va_arg(ap, uint64_t);
    const char * const *match_sym_list = va_arg(ap, const char * const *);
    int sd_rc;
    tlog_grc grc;

    /* Create JSON tokener */
    journal_json_reader->tok = json_tokener_new();
    if (journal_json_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    /* Open journal */
    sd_rc = sd_journal_open(&journal_json_reader->journal, 0);
    if (sd_rc < 0) {
        grc = TLOG_GRC_FROM(systemd, sd_rc);
        goto error;
    }

    /* Add matches */
    sd_rc = tlog_journal_add_match_sym_list(journal_json_reader->journal,
                                            match_sym_list);
    if (sd_rc < 0) {
        grc = TLOG_GRC_FROM(systemd, sd_rc);
        goto error;
    }

    /* Seek to "since" timestamp */
    sd_rc = sd_journal_seek_realtime_usec(journal_json_reader->journal,
                                          since);
    if (sd_rc < 0) {
        grc = TLOG_GRC_FROM(systemd, sd_rc);
        goto error;
    }

    /* Store "until" timestamp */
    journal_json_reader->until = until;

    return TLOG_RC_OK;

error:
    tlog_journal_json_reader_cleanup(reader);
    return grc;
}

static bool
tlog_journal_json_reader_is_valid(const struct tlog_json_reader *reader)
{
    struct tlog_journal_json_reader *journal_json_reader =
                                (struct tlog_journal_json_reader*)reader;
    return journal_json_reader->tok != NULL &&
           journal_json_reader->journal != NULL;
}

static size_t
tlog_journal_json_reader_loc_get(const struct tlog_json_reader *reader)
{
    return ((struct tlog_journal_json_reader*)reader)->entry;
}

static char *
tlog_journal_json_reader_loc_fmt(const struct tlog_json_reader *reader,
                            size_t loc)
{
    char *str;
    (void)reader;
    return asprintf(&str, "entry %zu", loc) >= 0 ? str : NULL;
}

tlog_grc
tlog_journal_json_reader_read(struct tlog_json_reader *reader,
                              struct json_object **pobject)
{
    struct tlog_journal_json_reader *journal_json_reader =
                                (struct tlog_journal_json_reader*)reader;
    tlog_grc grc;
    int sd_rc;
    struct json_object *object = NULL;

    /* If we ran out of time limit */
    if (journal_json_reader->last > journal_json_reader->until) {
        goto exit;
    }

    /* Advance to the next entry */
    sd_rc = sd_journal_next(journal_json_reader->journal);
    /* If failed */
    if (sd_rc < 0) {
        grc = TLOG_GRC_FROM(systemd, sd_rc);
        goto cleanup;
    /* If got an entry */
    } else if (sd_rc > 0) {
        const char *field_ptr;
        size_t field_len;
        const char *message_ptr;
        size_t message_len;

        /* Advance entry counter */
        journal_json_reader->entry++;

        /* Get the entry realtime timestamp */
        sd_rc = sd_journal_get_realtime_usec(journal_json_reader->journal,
                                             &journal_json_reader->last);
        if (sd_rc < 0) {
            grc = TLOG_GRC_FROM(systemd, sd_rc);
            goto cleanup;
        }
        if (journal_json_reader->last > journal_json_reader->until) {
            goto exit;
        }

        /* Get the entry message field data */
        sd_rc = sd_journal_get_data(journal_json_reader->journal, "MESSAGE",
                                    (const void **)&field_ptr, &field_len);
        if (sd_rc < 0) {
            grc = TLOG_GRC_FROM(systemd, sd_rc);
            goto cleanup;
        }

        /* Extract the message */
        message_ptr = (const char *)memchr(field_ptr, '=', field_len);
        if (message_ptr == NULL) {
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }
        message_ptr++;
        message_len = field_len - (message_ptr - field_ptr);

        /* Parse the message */
        object = json_tokener_parse_ex(journal_json_reader->tok,
                                       message_ptr, message_len);
        if (object == NULL) {
            grc = TLOG_GRC_FROM(
                    json, json_tokener_get_error(journal_json_reader->tok));
            goto cleanup;
        }
    }

exit:
    *pobject = object;
    object = NULL;
    grc = TLOG_RC_OK;
cleanup:
    json_object_put(object);
    return grc;
}

const struct tlog_json_reader_type tlog_journal_json_reader_type = {
    .size       = sizeof(struct tlog_journal_json_reader),
    .init       = tlog_journal_json_reader_init,
    .is_valid   = tlog_journal_json_reader_is_valid,
    .loc_get    = tlog_journal_json_reader_loc_get,
    .loc_fmt    = tlog_journal_json_reader_loc_fmt,
    .read       = tlog_journal_json_reader_read,
    .cleanup    = tlog_journal_json_reader_cleanup,
};
