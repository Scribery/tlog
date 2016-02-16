/**
 * @file
 * @brief Tlog-rec JSON configuration validation.
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

#ifndef _TLOG_REC_CONF_CMD_H
#define _TLOG_REC_CONF_CMD_H

#include <tlog/grc.h>
#include <json.h>
#include <stdio.h>

/**
 * Output a tlog-rec command-line help message to a stream.
 *
 * @param stream    The stream to output the message to.
 * @param progname  The program name to refer to in the help message.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_conf_cmd_help(FILE *stream, const char *progname);

/**
 * Load tlog-rec configuration from the command line.
 *
 * @param pconf Location for the pointer to the JSON object representing
 *              the loaded configuration.
 * @param argc  Tlog-rec argc value.
 * @param argc  Tlog-rec argv value.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_conf_cmd_load(struct json_object **pconf,
                                       int argc, char **argv);

#endif /* _TLOG_REC_CONF_CMD_H */
