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

#endif /* _TLOG_JSON_MISC_H */
