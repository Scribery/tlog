/**
 * @file
 * @brief Elasticsearch JSON message reader.
 *
 * An implementation of a reader retrieving JSON log messages from
 * Elasticsearch, provided base URL and a query.
 */
/*
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

#ifndef _TLOG_ES_JSON_READER_H
#define _TLOG_ES_JSON_READER_H

#include <assert.h>
#include <tlog/json_reader.h>

/**
 * Minimum number of messages to request from Elasticsearch in one HTTP
 * request
 */
#define TLOG_ES_JSON_READER_SIZE_MIN 1

/**
 * Elasticsearch message reader type
 *
 * Creation arguments:
 *
 * const char  *base_url    The base URL to request Elasticsearch, without the
 *                          query or the fragment parts.
 * const char  *query       The query string to send to ElastiSearch.
 * size_t       size        Number of messages to request from Elasticsearch
 *                          in one HTTP request.
 *
 */
extern const struct tlog_json_reader_type tlog_es_json_reader_type;

/**
 * Check if a base URL is valid for use with an Elasticsearch reader, that is,
 * if it doesn't contain the query (?...) or the fragment (#...) parts.
 *
 * @param base_url   The base URL to check.
 *
 * @return True if the base URL is valid, false otherwise.
 */
extern bool tlog_es_json_reader_base_url_is_valid(const char *base_url);

/**
 * Create an Elasticsearch reader.
 *
 * @param preader   Location for the created reader pointer, will be set to
 *                  NULL in case of error.
 * @param base_url  The base URL to request Elasticsearch, without the query
 *                  or the fragment parts.
 * @param query     The query string to send to ElastiSearch.
 * @param size      Number of messages to request from Elasticsearch in one
 *                  HTTP request.
 *
 * @return Global return code.
 */
static inline tlog_grc
tlog_es_json_reader_create(struct tlog_json_reader **preader,
                           const char *base_url,
                           const char *query,
                           size_t size)
{
    assert(preader != NULL);
    assert(tlog_es_json_reader_base_url_is_valid(base_url));
    assert(query != NULL);
    return tlog_json_reader_create(preader, &tlog_es_json_reader_type,
                                   base_url, query, size);
}

#endif /* _TLOG_ES_JSON_READER_H */
