/**
 * @file
 * @brief Tlog-rec-session command line configuration management.
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

#ifndef _TLOG_REC_SESSION_CONF_CMD_H
#define _TLOG_REC_SESSION_CONF_CMD_H

#include <tlog/errs.h>
#include <tlog/grc.h>
#include <json.h>
#include <stdio.h>

/**
 * Load tlog-rec-session configuration from the command line and extract
 * program name.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param phelp     Location for the dynamically-allocated usage help message.
 *                  Cannot be NULL.
 * @param pconf     Location for the pointer to the JSON object representing
 *                  the loaded configuration. Cannot be NULL.
 * @param argc      Tlog-rec-session argc value.
 * @param argv      Tlog-rec-session argv value. Cannot be NULL.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_session_conf_cmd_load(struct tlog_errs **perrs,
                                               char **phelp,
                                               struct json_object **pconf,
                                               int argc, char **argv);

#endif /* _TLOG_REC_SESSION_CONF_CMD_H */
