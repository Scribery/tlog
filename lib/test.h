/*
 * Tlog test functions.
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

#ifndef _TLOG_TEST_H
#define _TLOG_TEST_H

#include <stdio.h>
#include <stdint.h>

/**
 * Output a description of differences between two buffers to an stdio stream.
 *
 * @param stream    The stdio stream to output to.
 * @param res       "Result" buffer.
 * @param exp       "Expected" buffer.
 * @param len       Length to compare.
 */
extern void tlog_test_diff(FILE *stream, const uint8_t *res,
                           const uint8_t *exp, size_t len);

#endif /* _TLOG_TEST_H */
