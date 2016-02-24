m4_include(`misc.m4')m4_dnl
/*
 * Tlog-rec command-line parsing.
 *
m4_generated_warning(` * ')m4_dnl
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

#include <tlog/rec_conf_validate.h>
#include <tlog/rec_conf_cmd.h>
#include <tlog/json_misc.h>
#include <tlog/rc.h>
#include <tlog/misc.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <inttypes.h>

/** Option codes */
enum tlog_rec_conf_cmd_opt {
m4_divert(-1)
m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$6',
            `',
            ,
            `
                m4_ifelse(
                    `$3',
                    `args',
                    ,
                    `
                        m4_print(
                            `    TLOG_REC_CONF_CMD_OPT',
                            m4_translit(m4_translit(`$1/$2', `/', `_'), `a-z', `A-Z'),
                            ` = ',
                            `m4_singlequote(`$6')')
                        m4_printl(`,')
                    '
                )
            '
        )
    '
)
m4_include(`rec_conf_schema.m4')
m4_define(`M4_FIRST', `true')
m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$6',
            `',
            `
                m4_ifelse(
                    `$3',
                    `args',
                    ,
                    `
                        m4_print(
                            `    TLOG_REC_CONF_CMD_OPT',
                            m4_translit(m4_translit(`$1/$2', `/', `_'), `a-z', `A-Z'))
                        m4_ifelse(
                            M4_FIRST(),
                            `true',
                            `
                                m4_print(` = 0x100')
                                m4_define(`M4_FIRST', `false')
                            '
                        )
                        m4_printl(`,')
                    '
                )
            '
        )
    '
)
m4_include(`rec_conf_schema.m4')
m4_divert(0)};

/** Description of short options */
static const char *tlog_rec_conf_cmd_shortopts = "m4_dnl
m4_divert(-1)
m4_define(`M4_TYPE_INT', `:')
m4_define(`M4_TYPE_STRING', `:')
m4_define(`M4_TYPE_BOOL', `::')
m4_define(`M4_TYPE_CHOICE', `:')
m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$3',
            `args',
            ,
            `
                m4_ifelse(
                    `$6',
                    `',
                    ,
                    `
                        m4_print(`$6'$4)
                    '
                )
            '
        )
    '
)
m4_include(`rec_conf_schema.m4')
m4_divert(0)";

/** Description of long options */
static const struct option tlog_rec_conf_cmd_longopts[] = {
m4_divert(-1)
m4_define(`M4_TYPE_INT',    `required_argument')
m4_define(`M4_TYPE_STRING', `required_argument')
m4_define(`M4_TYPE_BOOL',   `optional_argument')
m4_define(`M4_TYPE_CHOICE', `required_argument')
m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$3',
            `args',
            ,
            `
                m4_printl(
                   `    {',
                   `        .name = "m4_substr(m4_translit(`$1/$2', `/', `-'), 1)",')
                m4_print(
                   `        .val = TLOG_REC_CONF_CMD_OPT',
                   m4_translit(m4_translit(`$1/$2', `/', `_'), `a-z', `A-Z'))
                m4_printl(
                   `,',
                   `        .has_arg = $4,',
                   `    },')
            '
        )
    '
)
m4_include(`rec_conf_schema.m4')
m4_divert(0)m4_dnl
    {
        .name = NULL
    }
};

tlog_grc
tlog_rec_conf_cmd_help(FILE *stream, const char *progname)
{
    const char *fmt =
       "Usage: %s [OPTION...] [CMD_FILE [CMD_ARG...]]\n"
       "   or: %s -c [OPTION...] CMD_STRING [CMD_NAME [CMD_ARG...]]\n"
       "Start a shell and log terminal I/O.\n"
m4_divert(-1)

m4_define(`M4_PREFIX', `')

m4_dnl
m4_dnl Output an option description
m4_dnl  Arguments:
m4_dnl
m4_dnl      $1  Option signature
m4_dnl      $2  Option description
m4_dnl
m4_define(
    `M4_PARAM_OPT',
    `
        m4_print(
           `       "    ',
           `$1',
           m4_substr(`                            ', m4_len(`$1')),
           `$2')
        m4_printl(`\n"')
    '
)

m4_define(
    `M4_CONTAINER_PARAM',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    `$3',
                    `args',
                    ,
                    `
                        M4_PARAM_OPT(
                            m4_ifelse(`$6',,, `-$6`,' ')--m4_substr(m4_translit(`$1/$2', `/', `-'), 1)`$7',
                            `$8')
                    '
                )
            '
        )
    '
)

m4_pushdef(
    `M4_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_printl(
                   `       "\n"',
                   `       "$3 options:\n"')
                m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')

                m4_pushdef(`M4_CONTAINER', `')
                m4_pushdef(`M4_PARAM', m4_defn(`M4_CONTAINER_PARAM'))
                m4_include(`rec_conf_schema.m4')
                m4_popdef(`M4_PARAM')
                m4_popdef(`M4_CONTAINER')

                m4_pushdef(`M4_PARAM', `')
                m4_include(`rec_conf_schema.m4')
                m4_popdef(`M4_PARAM')

                m4_popdef(`M4_PREFIX')
            '
        )
    '
)

M4_CONTAINER(`', `', `General')

m4_popdef(`M4_CONTAINER')

m4_divert(0)m4_dnl
       "\n";
    if (fprintf(stream, fmt, progname, progname) < 0) {
        return TLOG_GRC_ERRNO;
    } else {
        return TLOG_RC_OK;
    }
}

tlog_grc
tlog_rec_conf_cmd_load(struct json_object **pconf, int argc, char **argv)
{
    tlog_grc grc;
    char *progpath = NULL;
    const char *progname;
    struct json_object *conf = NULL;
    int optcode;
    const char *optname;
    const char *optpath;
    int64_t val_int;
    struct json_object *val = NULL;
    struct json_object *args = NULL;
    int end;

    /* Create empty configuration */
    conf = json_object_new_object();
    if (conf == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed creating configuration object: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }

    /* Extract program name, noting login dash prefix */
    progpath = strdup(argv[0]);
    if (progpath == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed allocating a copy of program path: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    progname = basename(progpath);
    if (progname[0] == m4_singlequote(-)) {
        progname++;
        val = json_object_new_boolean(true);
        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed creating login flag: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        grc = tlog_json_object_object_add_path(conf, "login", val);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed storing login flag: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        val = NULL;
    }

    /* Store stripped program name */
    val = json_object_new_string(progname);
    if (val == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed creating program name object: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    grc = tlog_json_object_object_add_path(conf, "progname", val);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed storing program name: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    val = NULL;

    /* Read all options */
    while ((optcode = getopt_long(argc, argv,
                                  tlog_rec_conf_cmd_shortopts, 
                                  tlog_rec_conf_cmd_longopts,
                                  NULL)) >= 0) {
        switch (optcode) {
m4_divert(-1)
m4_define(
    `M4_TYPE_INT',
    `
        m4_printl(
           `            if (sscanf(optarg, "%" SCNd64 " %n", &val_int, &end) < 1 ||',
           `                optarg[end] != 0 || val_int < $2) {',
           `                fprintf(stderr, "Invalid %s option value: %s\n",',
           `                        optname, optarg);',
           `                tlog_rec_conf_cmd_help(stderr, progname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            val = json_object_new_int64(val_int);')
    '
)

m4_define(
    `M4_TYPE_STRING',
    `
        m4_printl(
           `            val = json_object_new_string(optarg);')
    '
)

m4_define(
    `M4_TYPE_BOOL',
    `
        m4_printl(
           `            if (optarg == NULL ||',
           `                strcasecmp(optarg, "yes") == 0 ||',
           `                strcasecmp(optarg, "on") == 0 ||',
           `                strcasecmp(optarg, "true") == 0) {',
           `                val = json_object_new_boolean(true);',
           `            } else if (strcasecmp(optarg, "no") == 0 ||',
           `                       strcasecmp(optarg, "off") == 0 ||',
           `                       strcasecmp(optarg, "false") == 0) {',
           `                val = json_object_new_boolean(false);',
           `            } else {',
           `                fprintf(stderr,',
           `                        "Invalid %s option value: %s,\n"',
           `                        "expecting yes/on/true or no/off/false.\n",',
           `                        optname, optarg);',
           `                tlog_rec_conf_cmd_help(stderr, progname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }')
    '
)

m4_define(
    `M4_TYPE_CHOICE_LIST',
    `
        m4_ifelse(`$#', `0', `',
                  `$#', `1', `m4_print(`"$1"')',
                  `
                    m4_printl(`"$1"`,'')
                    m4_print(`                                      ')
                    M4_TYPE_CHOICE_LIST(m4_shift($@))
                  '
        )
    '
)

m4_define(
    `M4_TYPE_CHOICE',
    `
        m4_printl(
           `            {')
        m4_print(
           `                const char *list[] = {')
        M4_TYPE_CHOICE_LIST(m4_shift($@))
        m4_printl(
           `};',
           `                size_t i;',
           `                for (i = 0;',
           `                     i < TLOG_ARRAY_SIZE(list) && strcmp(optarg, list[i]) != 0;',
           `                     i++);',
           `                if (i >= TLOG_ARRAY_SIZE(list)) {',
           `                    fprintf(stderr, "Invalid %s option value: %s\n",',
           `                            optname, optarg);',
           `                    grc = TLOG_RC_FAILURE;',
           `                    tlog_rec_conf_cmd_help(stderr, progname);',
           `                    goto cleanup;',
           `                }',
           `            }',
           `            val = json_object_new_string(optarg);')
    '
)

m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$3',
            `args',
            ,
            `
                m4_print(
                   `        case TLOG_REC_CONF_CMD_OPT',
                   m4_translit(m4_translit(`$1/$2', `/', `_'), `a-z', `A-Z'))
                m4_printl(
                    `:')
                m4_print(
                   `            optname = "',
                   m4_ifelse(`$6', `', , `-$6/'),
                   `--',
                   m4_substr(m4_translit(`$1/$2', `/', `-'), 1),
                   `"')
                m4_printl(`;')
                m4_print(
                   `            optpath = "',
                   m4_substr(m4_translit(`$1/$2', `/', `.'), 1),
                   `"')
                m4_printl(
                   `;')
                $4
                m4_printl(
                   `            break;',
                   `')
            '
        )
    '
)
m4_include(`rec_conf_schema.m4')
m4_divert(0)m4_dnl
        case m4_singlequote(`?'):
            grc = TLOG_RC_FAILURE;
            tlog_rec_conf_cmd_help(stderr, progname);
            goto cleanup;

        default:
            fprintf(stderr, "Unknown option code: %d\n", optcode);
            grc = TLOG_RC_FAILURE;
            goto cleanup;
        }

        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed creating %s option value: %s\n",
                    optname, tlog_grc_strerror(grc));
            goto cleanup;
        }
        grc = tlog_json_object_object_add_path(conf, optpath, val);
        if (grc != TLOG_RC_OK) {
            fprintf(stderr, "Failed storing %s option value: %s\n",
                    optname, tlog_grc_strerror(grc));
            goto cleanup;
        }
        val = NULL;
    }

    /* Add other arguments */
    args = json_object_new_array();
    if (args == NULL) {
        grc = TLOG_GRC_ERRNO;
        fprintf(stderr, "Failed creating positional argument list: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    for (int i = 0; optind < argc; i++, optind++) {
        val = json_object_new_string(argv[optind]);
        if (val == NULL) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed creating argument value: %s\n",
                    tlog_grc_strerror(grc));
            goto cleanup;
        }
        if (json_object_array_put_idx(args, i, val) < 0) {
            grc = TLOG_GRC_ERRNO;
            fprintf(stderr, "Failed storing argument #%d: %s\n",
                    i, tlog_grc_strerror(grc));
            goto cleanup;
        }
        val = NULL;
    }
    grc = tlog_json_object_object_add_path(conf, "args", args);
    if (grc != TLOG_RC_OK) {
        fprintf(stderr, "Failed storing argument list: %s\n",
                tlog_grc_strerror(grc));
        goto cleanup;
    }
    args = NULL;

    /* Validate the result */
    grc = tlog_rec_conf_validate(conf, TLOG_REC_CONF_ORIGIN_ARGS);
    if (grc != TLOG_RC_OK) {
        goto cleanup;
    }

    *pconf = conf;
    conf = NULL;
    grc = TLOG_RC_OK;

cleanup:
    free(progpath);
    json_object_put(args);
    json_object_put(val);
    json_object_put(conf);
    return grc;
}
