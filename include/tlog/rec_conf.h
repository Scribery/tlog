/**
 * @file
 * @brief Tlog-rec configuration parsing
 */
/*
 *
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

#ifndef _TLOG_REC_CONF_H
#define _TLOG_REC_CONF_H

#include <tlog/grc.h>
#include <json.h>

/**
 * Load tlog-rec configuration from various sources and extract program name.
 *
 * @param pcmd_help Location for the dynamically-allocated command-line usage
 *                  help message. Cannot be NULL.
 * @param pconf     Location for the pointer to the JSON object representing
 *                  the loaded configuration. Cannot be NULL.
 * @param argc      Tlog-rec argc value.
 * @param argv      Tlog-rec argv value. Cannot be NULL.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_conf_load(char **pcmd_help,
                                   struct json_object **pconf,
                                   int argc, char **argv);

/**
 * Get shell parameters from a loaded tlog-rec configuration JSON object.
 *
 * @param conf  Tlog-rec configuration JSON object.
 * @param ppath Location for the shell path pointer, references the
 *              configuration object.
 * @param pargv Location for the NULL-terminated shell argv array, to be
 *              freed, both the array and the elements.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_conf_get_shell(struct json_object *conf,
                                        const char **ppath,
                                        char ***pargv);

#endif /* _TLOG_REC_CONF_H */
