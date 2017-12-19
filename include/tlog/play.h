/**
 * @file
 * @brief Generic playback process
 */
/*
 * Copyright (C) 2017 Red Hat
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

#ifndef _TLOG_PLAY_H
#define _TLOG_PLAY_H

#include <tlog/grc.h>
#include <tlog/errs.h>
#include <json.h>

/**
 * Run playback.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param cmd_help  Command-line usage help message.
 * @param conf      Configuration JSON object.
 * @param psignal   Location for the number of the signal which caused
 *                  playback to exit, zero if none. Not modified in case of
 *                  error. Can be NULL.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_play(struct tlog_errs **perrs,
                          const char *cmd_help,
                          struct json_object *conf,
                          int *psignal);

#endif /* _TLOG_PLAY_H */
