/**
 * @file
 * @brief Miscellaneous systemd journal-related functions.
 *
 * A collection of miscellaneous systemd journal-related function.
 *
 * This module also operates on "match symbols" which is a superset of journal
 * match strings described in sd_journal_add_match(3), with two special
 * strings added: "AND" and "OR", which correspond to conjunction and
 * disjunction respectively, as added by sd_journal_add_conjunction(3) and
 * sd_journal_add_disjunction(3) functions.
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

#ifndef _TLOG_JOURNAL_MISC_H
#define _TLOG_JOURNAL_MISC_H

#include <stdbool.h>
#include <stdlib.h>
#include <systemd/sd-journal.h>

/**
 * Check if a journal match is valid according to requirements listed in
 * sd_journal_add_match(3).
 *
 * @param data  Location of the match to check. Cannot be NULL.
 * @param size  Size of the match to check.
 *              If zero, data is assumed to be zero-terminated.
 *
 * @return True if the match is valid, false otherwise.
 */
extern bool tlog_journal_match_is_valid(const void *data, size_t size);

/**
 * Check if a journal match symbol is valid according to requirements listed
 * in sd_journal_add_match(3), or is either "OR", or "AND" string.
 *
 * @param match_sym A match symbol to check.
 *
 * @return True if the match symbol is valid, false otherwise.
 */
extern bool tlog_journal_match_sym_is_valid(const char *match_sym);

/**
 * Add a match symbol to a journal context.
 *
 * @param j         The journal context to add the match symbol to.
 * @param match_sym The match symbol to add to the journal context.
 *                  Must be valid.
 *
 * @return An sd_journal_add_match/disjunction/conjunction(3) return code.
 */
extern int tlog_journal_add_match_sym(sd_journal *j, const char *match_sym);

/**
 * Check if a NULL-terminated list of journal match symbols is valid, i.e. it
 * is not NULL, and each item is valid according to requirements listed in
 * sd_journal_add_match(3), or is either "OR" or "AND" string.
 *
 * @param match_sym_list    The match symbol list to check.
 *
 * @return True if the list is valid, false otherwise.
 */
extern bool tlog_journal_match_sym_list_is_valid(const char * const *match_sym_list);

/**
 * Add a NULL-terminated match symbol list to a journal context.
 *
 * @param j                 The journal context to add the match symbol to.
 * @param match_sym_list    The match symbol list to add to the journal context.
 *                          Nothing is added if NULL.
 *                          Must be valid if not NULL.
 *
 * @return An sd_journal_add_match/disjunction/conjunction(3) return code.
 *         If the return code indicates error, the match symbol list might
 *         have been partially added.
 */
extern int tlog_journal_add_match_sym_list(sd_journal *j,
                                           const char * const *match_sym_list);

#endif /* _TLOG_JOURNAL_MISC_H */
