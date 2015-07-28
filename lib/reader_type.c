/*
 * Tlog message reader type.
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "tlog/reader_type.h"

bool
tlog_reader_type_is_valid(const struct tlog_reader_type *type)
{
    return type != NULL &&
           type->size != 0 &&
           type->init != NULL &&
           (type->loc_fmt == NULL || type->loc_get != NULL) &&
           type->read != NULL;
}

char *
tlog_reader_type_strerror(const struct tlog_reader_type *type, int error)
{
    assert(tlog_reader_type_is_valid(type));

    if (error == 0) {
        return strdup("Success");
    } if (error < 0) {
        return strdup(strerror(-error));
    } else if (type->strerror != NULL) {
        return type->strerror(error);
    } else {
        char *str = NULL;
        asprintf(&str, "Error code %d", error);
        return str;
    }
}

char *
tlog_reader_type_loc_fmt(const struct tlog_reader_type *type, size_t loc)
{

    assert(tlog_reader_type_is_valid(type));
    if (type->loc_fmt != NULL) {
        return type->loc_fmt(loc);
    } else if (type->loc_get != NULL) {
        char *str = NULL;
        asprintf(&str, "location %zu", loc);
        return str;
    } else {
        return strdup("unknown location");
    }
}
