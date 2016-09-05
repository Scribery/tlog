/**
 * @file
 * @brief Tlog-play configuration parsing
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

#ifndef _TLOG_PLAY_CONF_H
#define _TLOG_PLAY_CONF_H

#include <tlog/grc.h>
#include <json.h>

/**
 * Load tlog-play configuration from various sources and extract program name.
 *
 * @param pcmd_help Location for the dynamically-allocated command-line usage
 *                  help message.
 * @param pconf     Location for the pointer to the JSON object representing
 *                  the loaded configuration.
 * @param argc      Tlog-play argc value.
 * @param argv      Tlog-play argv value.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_play_conf_load(char **pcmd_help,
                                    struct json_object **pconf,
                                    int argc, char **argv);

#endif /* _TLOG_PLAY_CONF_H */
