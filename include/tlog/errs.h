/**
 * @file
 * @brief Error stack.
 *
 * An error stack is used to return messages accumulated during error return
 * through one or more function stack frames, so the caller can present the
 * user with detailed diagnostics, down to the lowest level, as opposed to
 * just the topmost return code.
 *
 * An empty error stack is just a NULL pointer, so the sucessfull execution
 * doesn't cost anything. Errors are added to the stack using tlog_err_push*
 * functions.
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

#ifndef _TLOG_ERRS_H
#define _TLOG_ERRS_H

#include <stdio.h>
#include <stdarg.h>
#include <tlog/grc.h>
#include <tlog/rc.h>

/** An error stack item (an error) */
struct tlog_errs {
    struct tlog_errs   *next;   /**< Next (lower-level) error */
    char               *msg;    /**< This error's message,
                                     dynamically allocated */
};

/** An empty error stack */
#define TLOG_ERRS_EMPTY    NULL

/**
 * A single-item stack used in place of a stack which failed to extend due to
 * an allocation error. It is never destroyed by tlog_errs_destroy.
 */
extern const struct tlog_errs tlog_errs_nomem;

/**
 * Destroy an error stack.
 *
 * @param errs  Location of the error stack to destroy, cannot be NULL.
 *              Location will be written NULL (empty stack) after destruction.
 */
extern void tlog_errs_destroy(struct tlog_errs **perrs);

/**
 * Push a stack to another stack.
 *
 * @param perrs Location of the stack to push to. The location is not modified
 *              if the pushed stack is empty (NULL). Destroyed and replaced
 *              with the pushed stack, if the latter ends with
 *              tlog_errs_nomem. If NULL, then the pushed stack is destroyed.
 * @param errs  The stack to push.
 */
extern void tlog_errs_push(struct tlog_errs **perrs,
                           struct tlog_errs *errs);

/**
 * Push an error to a stack, with message as a constant string.
 *
 * @param perrs Location of the stack to push the error to.
 *              If memory allocation fails, location is replaced with
 *              tlog_errs_nomem and the original stack is freed.
 *              If NULL, then nothing is done.
 * @param str   The message string to copy and put into the pushed error.
 *              Cannot be NULL.
 */
extern void tlog_errs_pushs(struct tlog_errs **perrs, const char *msg);

/**
 * Push an error to a stack, with message being a global return code
 * description.
 *
 * @param perrs Location of the stack to push the error to.
 *              If memory allocation fails, location is replaced with
 *              tlog_errs_nomem and the original stack is freed.
 *              If NULL, then nothing is done.
 * @param grc   The global return code, which description will be pushed to
 *              the stack.
 */
extern void tlog_errs_pushc(struct tlog_errs **perrs, tlog_grc grc);

/**
 * Push an error to an error stack, with a sprintf-formatted message with
 * arguments from a va_list.
 *
 * @param perrs Location of the stack to push the error to.
 *              If memory allocation fails, location is replaced with
 *              tlog_errs_nomem and the original stack is freed.
 *              If NULL, then nothing is done.
 * @param fmt   The message sprintf(3) format string. Cannot be NULL.
 * @param ap    The message format arguments.
 */
extern void tlog_errs_pushvf(struct tlog_errs **perrs,
                             const char *fmt, va_list ap);

/**
 * Push an error to an error stack, with a sprintf-formatted message.
 *
 * @param perrs Location of the stack to push the error to.
 *              If memory allocation fails, location is replaced with
 *              tlog_errs_nomem and the original stack is freed.
 *              If NULL, then nothing is done.
 * @param fmt   The message sprintf(3) format string. Cannot be NULL.
 * @param ...   The message format arguments.
 */
extern void tlog_errs_pushf(struct tlog_errs **perrs, const char *fmt, ...)
                           __attribute__((format (printf, 2, 3)));

/**
 * Print all errors from a stack to an stdio stream, from first-pushed
 * to last-pushed. Each error's message is appended with a newline.
 *
 * @param stream    The FILE stream to print the stack to.
 *                  Cannot be NULL.
 * @param errs      The stack to print. Can be NULL for empty stack.
 *
 * @return Number of characters printed, or a negative value on error, with
 *         errno set.
 */
extern int tlog_errs_print(FILE *stream, const struct tlog_errs *errs);

/**
 * Push an error to the "perrs" stack, with message as a constant string,
 * and then go to cleanup.
 *
 * @param _msg   The message string to copy and put into the pushed error.
 *               Cannot be NULL.
 */
#define TLOG_ERRS_RAISES(_msg) \
     do {                                                                 \
         tlog_errs_pushs(perrs, _msg);                                    \
         goto cleanup;                                                    \
     } while (0)

/**
 * Push two errors to the "perrs" stack, first one with message being a global
 * return code description, and the second with message as a constant string.
 * Then go to cleanup.
 *
 * @param _grc  The global return code, which description will be pushed to
 *              the stack.
 * @param _msg  The message string to copy and put into the pushed error.
 *              Cannot be NULL.
 */
#define TLOG_ERRS_RAISECS(_grc, _msg) \
     do {                                                                 \
         tlog_errs_pushc(perrs, _grc);                                    \
         tlog_errs_pushs(perrs, _msg);                                    \
         goto cleanup;                                                    \
     } while (0)

/**
 * Push an error to the "perrs" stack, with a sprintf-formatted message.
 * Then go to cleanup.
 *
 * @param _fmt  The message sprintf(3) format string. Cannot be NULL.
 * @param ...   The message format arguments.
 */
#define TLOG_ERRS_RAISEF(_fmt, _args...) \
     do {                                                                 \
         tlog_errs_pushf(perrs, _fmt, ##_args);                           \
         goto cleanup;                                                    \
     } while (0)

/**
 * Push two errors to the "perrs" stack, first one with message being a global
 * return code description, and the second with a sprintf-formatted message.
 * Then go to cleanup.
 *
 * @param _grc  The global return code, which description will be pushed to
 *              the stack.
 * @param _fmt  The message sprintf(3) format string. Cannot be NULL.
 * @param ...   The message format arguments.
 */
#define TLOG_ERRS_RAISECF(_grc, _fmt, _args...) \
     do {                                                                 \
         tlog_errs_pushc(perrs, _grc);                                    \
         tlog_errs_pushf(perrs, _fmt, ##_args);                           \
         goto cleanup;                                                    \
     } while (0)

#endif /* _TLOG_ERRS_H */
