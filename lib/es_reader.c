/*
 * Tlog ElasticSearch message reader.
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

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <json_tokener.h>
#include <curl/curl.h>
#include "tlog/es_reader.h"
#include "tlog/rc.h"

struct tlog_es_reader {
    struct tlog_reader          reader;     /**< Base type */
    CURL                       *curl;       /**< libcurl handle */
    char                       *url_pfx;    /**< URL without the "from"
                                                 query field value */
    size_t                      size;       /**< Number of messages retrieved
                                                 in one request */
    struct json_tokener        *tok;        /**< JSON tokener object */
    struct json_object         *array;      /**< JSON array of retrieved
                                                 messages */
    size_t                      idx;        /**< Index of the message to be
                                                 read next */
};

bool
tlog_es_reader_base_url_is_valid(const char *base_url)
{
    return base_url != NULL &&
           strchr(base_url, '?') == NULL &&
           strchr(base_url, '#') == NULL;
}

static void
tlog_es_reader_cleanup(struct tlog_reader *reader)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    if (es_reader->array != NULL) {
        json_object_put(es_reader->array);
        es_reader->array = NULL;
    }
    if (es_reader->tok != NULL) {
        json_tokener_free(es_reader->tok);
        es_reader->tok = NULL;
    }
    free(es_reader->url_pfx);
    es_reader->url_pfx = NULL;
    if (es_reader->curl != NULL) {
        curl_easy_cleanup(es_reader->curl);
        es_reader->curl = NULL;
    }
}

/**
 * Format an ElasticSearch request URL prefix: the URL ending with "from=",
 * ready for the addition of the message index.
 *
 * @param purl_pfx  The location for the dynamically-allocated URL prefix.
 * @param curl      The CURL handle to escape with.
 * @param base_url  The base URL to request ElasticSearch, without the query
 *                  or the fragment parts.
 * @param query     The query string to send to ElastiSearch.
 * @param size      Number of messages to request from ElasticSearch in one
 *                  HTTP request.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_es_reader_format_url_pfx(char **purl_pfx,
                              CURL *curl,
                              const char *base_url,
                              const char *query,
                              size_t size)
{
    static const char *sort = "id:asc";
    static const char *fields = "type,"
                                "host,"
                                "user,"
                                "session,"
                                "id,"
                                "pos,"
                                "width,"
                                "height,"
                                "timing,"
                                "in_txt,"
                                "in_bin,"
                                "out_txt,"
                                "out_bin";

    tlog_grc grc;
    char *url_pfx;
    char *esc_query = NULL;
    char *esc_sort = NULL;
    char *esc_fields = NULL;

    assert(purl_pfx != NULL);
    assert(curl != NULL);
    assert(tlog_es_reader_base_url_is_valid(base_url));
    assert(query != NULL);
    assert(size >= TLOG_ES_READER_SIZE_MIN);

#define ESC(_token) \
    do {                                                    \
        esc_##_token = curl_easy_escape(curl, _token, 0);   \
        if (esc_##_token == NULL) {                         \
            grc = TLOG_GRC_FROM(errno, ENOMEM);             \
            goto cleanup;                                   \
        }                                                   \
    } while (0)

    ESC(query);
    ESC(sort);
    ESC(fields);

#undef ESC

    if (asprintf(&url_pfx,
                 "%s?q=%s&sort=%s&fields=%s&size=%zu&from=",
                 base_url, esc_query, esc_sort, esc_fields, size) < 0) {
        grc = TLOG_GRC_FROM(errno, ENOMEM);
        goto cleanup;
    }

    *purl_pfx = url_pfx;

    grc = TLOG_RC_OK;

cleanup:

    if (esc_fields != NULL) {
        curl_free(esc_fields);
    }
    if (esc_sort != NULL) {
        curl_free(esc_sort);
    }
    if (esc_query != NULL) {
        curl_free(esc_query);
    }
    return grc;
}

/** CURL write function data */
struct tlog_es_reader_write_data {
    struct json_tokener        *tok;    /**< JSON tokener object to use */
    enum json_tokener_error     rc;     /**< Return status of the tokener */
    struct json_object         *obj;    /**< The parsed JSON response */
};

/**
 * Parse the data retrieved by CURL into a JSON object - to be supplied to
 * curl_easy_setopt with CURLOPT_WRITEFUNCTION and called by
 * curl_easy_perform.
 *
 * @param ptr       Pointer to the retrieved (piece of) data that should be
 *                  parsed.
 * @param size      Size of each retrieved data chunk.
 * @param nmemb     Number of retrieved data chunks.
 * @param userdata  The data supplied to curl_easy_setopt as
 *                  CURLOPT_WRITEDATA, must be struct
 *                  tlog_es_reader_write_data.
 *
 * @return Number of bytes processed, signals error if different from
 *         size*nmemb, can be CURL_WRITEFUNC_PAUSE to signal transfer pause.
 */
static size_t
tlog_es_reader_write_func(char *ptr, size_t size, size_t nmemb,
                          void *userdata)
{
    struct tlog_es_reader_write_data *data =
                (struct tlog_es_reader_write_data *)userdata;
    size_t len = size * nmemb;

    assert(ptr != NULL || len == 0);
    assert(data != NULL);

    if (len == 0) {
        return len;
    }

    if (data->obj != NULL) {
        return !len;
    }

    data->obj = json_tokener_parse_ex(data->tok, ptr, (int)len);
    if (data->obj == NULL) {
        data->rc = json_tokener_get_error(data->tok);
        if (data->rc != json_tokener_continue) {
            return !len;
        }
    }

    return len;
}

static tlog_grc
tlog_es_reader_init(struct tlog_reader *reader, va_list ap)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    const char *base_url = va_arg(ap, const char *);
    const char *query = va_arg(ap, const char *);
    size_t size = va_arg(ap, size_t);
    CURLcode rc;
    tlog_grc grc;

    assert(tlog_es_reader_base_url_is_valid(base_url));
    assert(query != NULL);
    assert(size >= TLOG_ES_READER_SIZE_MIN);

    es_reader->curl = curl_easy_init();
    if (es_reader->curl == NULL) {
        grc = TLOG_RC_ES_READER_CURL_INIT_FAILED;
        goto error;
    }

    rc = curl_easy_setopt(es_reader->curl, CURLOPT_WRITEFUNCTION,
                          tlog_es_reader_write_func);
    if (rc != CURLE_OK) {
        grc = TLOG_GRC_FROM(curl, rc);
        goto error;
    }

    grc = tlog_es_reader_format_url_pfx(&es_reader->url_pfx,
                                        es_reader->curl,
                                        base_url, query, size);
    if (grc != TLOG_RC_OK) {
        goto error;
    }

    es_reader->size = size;

    es_reader->tok = json_tokener_new();
    if (es_reader->tok == NULL) {
        grc = TLOG_GRC_ERRNO;
        goto error;
    }

    es_reader->idx = 0;

    return TLOG_RC_OK;

error:
    tlog_es_reader_cleanup(reader);
    return grc;
}

static bool
tlog_es_reader_is_valid(const struct tlog_reader *reader)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    return es_reader->curl != NULL &&
           es_reader->url_pfx != NULL &&
           es_reader->size >= TLOG_ES_READER_SIZE_MIN &&
           es_reader->tok != NULL &&
           (es_reader->array != NULL ||
            es_reader->idx % es_reader->size == 0);
}

static size_t
tlog_es_reader_loc_get(const struct tlog_reader *reader)
{
    return ((struct tlog_es_reader*)reader)->idx;
}

static char *
tlog_es_reader_loc_fmt(size_t loc)
{
    char *str = NULL;
    asprintf(&str, "message #%zu", loc);
    return str;
}

/**
 * Format an ElasticSearch request URL for the current message index of a
 * reader.
 *
 * @param reader    The reader to format the URL for.
 * @param purl      The location for the dynamically-allocated URL.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_es_reader_format_url(struct tlog_reader *reader, char **purl)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    char *url;

    assert(tlog_es_reader_is_valid(reader));
    assert(es_reader->idx % es_reader->size == 0);

    if (asprintf(&url, "%s%zu", es_reader->url_pfx, es_reader->idx) < 0) {
        return TLOG_GRC_FROM(errno, ENOMEM);
    }

    *purl = url;

    return TLOG_RC_OK;
}

/**
 * Refill the retrieved message array of a reader.
 *
 * @param reader    The reader to refill the message array for.
 *
 * @return Global return code.
 */
static tlog_grc
tlog_es_reader_refill_array(struct tlog_reader *reader)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    tlog_grc grc;
    char *url = NULL;
    struct tlog_es_reader_write_data data;
    CURLcode rc;
    struct json_object *obj;

    assert(tlog_es_reader_is_valid(reader));
    assert(es_reader->idx % es_reader->size == 0);

    /* Free the previous array, if any */
    if (es_reader->array != NULL) {
        json_object_put(es_reader->array);
        es_reader->array = NULL;
    }

    /* Format request URL */
    grc = tlog_es_reader_format_url(reader, &url);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    /* Set request URL */
    rc = curl_easy_setopt(es_reader->curl, CURLOPT_URL, url);
    if (rc != CURLE_OK) {
        grc = TLOG_GRC_FROM(curl, rc);
        goto cleanup;
    }

    /* Set callback data */
    data.tok = es_reader->tok;
    data.rc = json_tokener_success;
    data.obj = NULL;
    rc = curl_easy_setopt(es_reader->curl, CURLOPT_WRITEDATA, &data);
    if (rc != CURLE_OK) {
        grc = TLOG_GRC_FROM(curl, rc);
        goto cleanup;
    }

    /* Perform the request */
    rc = curl_easy_perform(es_reader->curl);
    if (rc != CURLE_OK) {
        if (rc == CURLE_WRITE_ERROR && data.rc != json_tokener_success) {
            grc = TLOG_GRC_FROM(json, data.rc);
        } else {
            grc = TLOG_GRC_FROM(curl, rc);
        }
        goto cleanup;
    }

    /* If no data was read */
    if (data.obj == NULL) {
        /* Create an empty array */
        obj = json_object_new_array();
        if (obj == NULL) {
            grc = TLOG_GRC_FROM(errno, ENOMEM);
            goto cleanup;
        }
    } else {
        /* Extract the array */
        if (!json_object_object_get_ex(data.obj, "hits", &obj) ||
            !json_object_object_get_ex(obj, "hits", &obj) ||
            json_object_get_type(obj) != json_type_array) {
            grc = TLOG_RC_ES_READER_REPLY_INVALID;
            goto cleanup;
        }
        obj = json_object_get(obj);
    }

    es_reader->array = obj;

    grc = TLOG_RC_OK;

cleanup:

    if (data.obj != NULL) {
        json_object_put(data.obj);
    }
    free(url);
    return grc;
}

tlog_grc
tlog_es_reader_read(struct tlog_reader *reader, struct json_object **pobject)
{
    struct tlog_es_reader *es_reader =
                                (struct tlog_es_reader*)reader;
    tlog_grc grc;
    size_t array_idx = es_reader->idx % es_reader->size;
    struct json_object *object;

    /* If we're at the edge of the next window array */
    if (array_idx == 0) {
        /* Refill the message array */
        grc = tlog_es_reader_refill_array(reader);
        if (grc != TLOG_RC_OK) {
            return grc;
        }
    }

    /* Get next object from the array */
    object = json_object_array_get_idx(es_reader->array, array_idx);
    if (object != NULL) {
        es_reader->idx++;
    }

    *pobject = object;

    return TLOG_RC_OK;
}

const struct tlog_reader_type tlog_es_reader_type = {
    .size       = sizeof(struct tlog_es_reader),
    .init       = tlog_es_reader_init,
    .is_valid   = tlog_es_reader_is_valid,
    .loc_get    = tlog_es_reader_loc_get,
    .loc_fmt    = tlog_es_reader_loc_fmt,
    .read       = tlog_es_reader_read,
    .cleanup    = tlog_es_reader_cleanup,
};
