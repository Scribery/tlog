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

#ifndef _TLOG_REC_CONF_VALIDATE_H
#define _TLOG_REC_CONF_VALIDATE_H

#include <json.h>
#include <tlog/grc.h>
#include <tlog/conf_origin.h>

/**
 * Check tlog-rec JSON configuration: if there are no unkown nodes and the
 * present node types and values are valid.
 *
 * @param conf      The configuration JSON object to check.
 * @param origin    The configuration origin.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_rec_conf_validate(struct json_object *conf,
                                       enum tlog_conf_origin origin);

#endif /* _TLOG_REC_CONF_VALIDATE_H */
