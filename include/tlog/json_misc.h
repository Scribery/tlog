/**
 * @file
 * @brief Miscellaneous JSON handling functions.
 *
 * A collection of miscellaneous function for manipulating JSON objects.
 */
/*
 * Copyright (C) 2016 Red Hat
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

#ifndef _TLOG_JSON_MISC_H
#define _TLOG_JSON_MISC_H

#include <string.h>
#include <json.h>
#include <tlog/grc.h>

/**
 * Overlay one JSON object with another.
 *
 * @param presult   Location of/for the result object pointer.
 *                  Json_object_put will be called on it before overwriting.
 * @param lower     The object to overlay.
 * @param upper     The object to overlay with.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_overlay(struct json_object    **presult,
                                  struct json_object     *lower,
                                  struct json_object     *upper);

/**
 * Add an object field to a json_object of type json_type_object, located at
 * the specified dot-separated path. Intermediate objects will be created as
 * necessary.
 *
 * @param obj   The object to begin lookup from.
 * @param path  The dot-separated path to add the object at.
 * @param val   The json_object or NULL member to associate with the give
 *              field.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_object_object_add_path(struct json_object* obj,
                                                 const char *path,
                                                 struct json_object *val);

/**
 * Read JSON from file, returning an error code, as opposed to
 * json_object_from_file which doesn't.
 *
 * @param pconf     Location for the loaded configuration.
 * @param path      Path to the file to load and parse.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_json_object_from_file(struct json_object **pconf,
                                           const char          *path);

/**
 * Escape a string buffer for use inside a JSON string value.
 *
 * @param out_ptr   The output buffer pointer, can be NULL, if out_len is 0.
 * @param out_len   The output buffer length. Must account for terminating
 *                  zero. If the output doesn't fit into the specified space,
 *                  then it is terminated after the last character that fits.
 *                  If zero, then nothing is written.
 * @param in_ptr    The input buffer pointer, must be valid UTF-8.
 * @param in_len    The input buffer length, must end the input string so that
 *                  it remains valid UTF-8.
 *
 * @return Number of bytes required for the full output, including the
 *         terminating zero.
 */
extern size_t tlog_json_esc_buf(char       *out_ptr,
                                size_t      out_len,
                                const char *in_ptr,
                                size_t      in_len);

/**
 * Escape a string for use inside a JSON string value.
 *
 * @param out_ptr   The output buffer pointer, can be NULL, if out_len is 0.
 * @param out_len   The output buffer length. Must account for terminating
 *                  zero. If the output doesn't fit into the specified space,
 *                  then it is terminated after the last character that fits.
 *                  If zero, then nothing is written.
 * @param in        The input string, must be valid UTF-8.
 *
 * @return Number of bytes required for the full output, including the
 *         terminating zero.
 */
static inline size_t
tlog_json_esc_str(char *out_ptr, size_t out_len, const char *in)
{
    return tlog_json_esc_buf(out_ptr, out_len,
                             in, strlen((const char *)in));
}

/**
 * Escape a string buffer for use inside a JSON string value, outputting to a
 * dynamically-allocated string.
 *
 * @param in_ptr    The input buffer pointer, must be valid UTF-8.
 * @param in_len    The input buffer length, must end the input string so that
 *                  it remains valid UTF-8.
 *
 * @return Pointer to the dynamically-allocated output string, or NULL if
 *         allocation failed.
 */
extern char *tlog_json_aesc_buf(const char *in_ptr, size_t in_len);


/**
 * Escape a string for use inside a JSON string value, outputting to a
 * dynamically-allocated string.
 *
 * @param in        The input string, must be valid UTF-8.
 *
 * @return Pointer to the dynamically-allocated output string, or NULL if
 *         allocation failed.
 */
static inline char *
tlog_json_aesc_str(const char *in)
{
    return tlog_json_aesc_buf(in, strlen((const char *)in));
}

#endif /* _TLOG_JSON_MISC_H */
