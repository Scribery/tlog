/**
 * @file
 * @brief Miscellaneous functions.
 *
 * A collection of miscellaneous function, macros and data types not worthy of
 * their own module.
 */
/*
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

#ifndef _TLOG_MISC_H
#define _TLOG_MISC_H

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <tlog/errs.h>
#include <tlog/grc.h>

/** Tlog version and license information */
extern const char *tlog_version;

/** Return number of elements in an array */
#define TLOG_ARRAY_SIZE(_x) (sizeof(_x) / sizeof((_x)[0]))

/** Return maximum of two numbers */
#define TLOG_MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))

/** Return minimum of two numbers */
#define TLOG_MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

/** Return an offset of a type member */
#define TLOG_OFFSET_OF(_type, _member) ((size_t) &((_type *)0)->_member)

/**
 * Cast a member of a structure out to the containing structure.
 *
 * @param \_ptr     The pointer to the member.
 * @param \_type    The type of the container this is embedded in.
 * @param \_member  The name of the member within the type.
 *
 */
#define TLOG_CONTAINER_OF(_ptr, _type, _member) \
    ({                                                              \
        const typeof(((_type *)0)->_member) *_mptr = (_ptr);        \
        (_type *)((char *)_mptr - TLOG_OFFSET_OF(_type, _member));  \
    })

/**
 * Convert a nibble (4-bit number) to a hexadecimal digit.
 *
 * @param n     Nibble to convert.
 *
 * @return Hexadecimal digit.
 */
static inline uint8_t
tlog_nibble_digit(uint8_t n)
{
    return (n < 10) ? ('0' + n) : ('a' - 10 + n);
}

/**
 * Calculate number of decimal digits in a size_t number.
 *
 * @param n     The number to count digits for.
 *
 * @return Number of digits.
 */
static inline size_t
tlog_size_digits(size_t n)
{
    size_t d;
    for (d = 1; n > 9; d++, n /= 10);
    return d;
}

/**
 * Retrieve an absolute path to a file either in the build tree, if possible,
 * and if running from the build tree, or at the installed location.
 *
 * @param ppath             Location for the retrieved absolute path.
 * @param prog_path         Path to a program relative to which the
 *                          build_rel_path is specified, to use if running
 *                          from the build tree.
 * @param build_rel_path    Build tree path relative to prog_path, to use if
 *                          running from the build tree.
 * @param inst_abs_path     Absolute installation path to use if not running
 *                          from the build tree, or if prog_path or
 *                          build_rel_path were not found.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_build_or_inst_path(char          **ppath,
                                        const char     *prog_path,
                                        const char     *build_rel_path,
                                        const char     *inst_abs_path);


/**
 * Evaluate an expression with specified EUID/EGID set temporarily.
 *
 * In case of an error setting/restoring EUID/EGID, sets the "grc" variable,
 * pushes messages to the specified error stack and jumps to "cleanup" label.
 *
 * @param _euid     The EUID to set temporarily.
 * @param _egid     The EGID to set temporarily.
 * @param _expr     The expression to evaluate with EUID/EGID set.
 */
#define TLOG_EVAL_WITH_EUID_EGID(_euid, _egid, _expr) \
    do {                                                                 \
        uid_t _orig_euid = geteuid();                                    \
        gid_t _orig_egid = getegid();                                    \
                                                                         \
        /* Set EUID temporarily */                                       \
        if (seteuid(_euid) < 0) {                                        \
            grc = TLOG_GRC_ERRNO;                                        \
            TLOG_ERRS_RAISECS(grc, "Failed setting EUID");               \
        }                                                                \
                                                                         \
        /* Set EGID temporarily */                                       \
        if (setegid(_egid) < 0) {                                        \
            grc = TLOG_GRC_ERRNO;                                        \
            TLOG_ERRS_RAISECS(grc, "Failed setting EGID");               \
        }                                                                \
                                                                         \
        /* Evaluate */                                                   \
        _expr;                                                           \
                                                                         \
        /* Restore EUID */                                               \
        if (seteuid(_orig_euid) < 0) {                                   \
            grc = TLOG_GRC_ERRNO;                                        \
            TLOG_ERRS_RAISECS(grc, "Failed restoring EUID");             \
        }                                                                \
                                                                         \
        /* Restore EGID */                                               \
        if (setegid(_orig_egid) < 0) {                                   \
            grc = TLOG_GRC_ERRNO;                                        \
            TLOG_ERRS_RAISECS(grc, "Failed restoring EGID");             \
        }                                                                \
    } while (0)

/** tlog_exec option bits (must be ascending powers of two) */
enum tlog_exec_opt {
    /** Search the PATH for the program to execute */
    TLOG_EXEC_OPT_SEARCH_PATH   = 0x01,
    /** Drop effective UID/GID before execution */
    TLOG_EXEC_OPT_DROP_PRIVS    = 0x02,
    /** Maximum option bit value plus one (not a valid bit) */
    TLOG_EXEC_OPT_MAX_PLUS_ONE
};

/** Bitmask with all tlog_exec_opt bits on */
#define TLOG_EXEC_OPT_MASK  (((TLOG_EXEC_OPT_MAX_PLUS_ONE - 1) << 1) - 1)

/**
 * Execute a file.
 *
 * @param perrs     Location for the error stack. Can be NULL.
 * @param opts      Options: a bitmask of TLOG_EXECT_OPT_* bits.
 * @param path      Path to the program to execute.
 * @param argv      ARGV array for the executed program.
 *
 * @return Global return code.
 */
extern tlog_grc tlog_exec(struct tlog_errs **perrs, unsigned int opts,
                          const char *path, char **argv);

#endif /* _TLOG_MISC_H */
