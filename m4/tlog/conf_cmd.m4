m4_dnl
m4_dnl Tlog m4 macros for generating command line parsing.
m4_dnl
m4_dnl Copyright (C) 2016 Red Hat
m4_dnl
m4_dnl This file is part of tlog.
m4_dnl
m4_dnl Tlog is free software; you can redistribute it and/or modify
m4_dnl it under the terms of the GNU General Public License as published by
m4_dnl the Free Software Foundation; either version 2 of the License, or
m4_dnl (at your option) any later version.
m4_dnl
m4_dnl Tlog is distributed in the hope that it will be useful,
m4_dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
m4_dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
m4_dnl GNU General Public License for more details.
m4_dnl
m4_dnl You should have received a copy of the GNU General Public License
m4_dnl along with tlog; if not, write to the Free Software
m4_dnl Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
m4_dnl
m4_dnl
m4_include(`misc.m4')m4_dnl
m4_include(`conf_origin.m4')m4_dnl
m4_include(`conf_misc.m4')m4_dnl
m4_divert(-1)

m4_dnl
m4_dnl Output a short option enum member, see the *schema.m4 for parameters.
m4_dnl
m4_define(
    `M4_CONF_CMD_OPT_ENUM_PARAM_SHORT',
    `
        m4_ifelse(
            `$6',
            `',
            ,
            `
                m4_ifelse(
                    m4_conf_origin_is_in_range(`opts', `$3'),
                    1,
                    `
                        m4_print(
                            `        OPT',
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

m4_dnl
m4_dnl Output a long option enum member, see the *schema.m4 for parameters.
m4_dnl Macros:
m4_dnl
m4_dnl  M4_FIRST - `true' if this is the first invocation, `false' otherwise.
m4_dnl
m4_define(
    `M4_CONF_CMD_OPT_ENUM_PARAM_LONG',
    `
        m4_ifelse(
            `$6',
            `',
            `
                m4_ifelse(
                    m4_conf_origin_is_in_range(`opts', `$3'),
                    1,
                    `
                        m4_print(
                            `        OPT',
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

m4_dnl
m4_dnl Output an option enumeration.
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_OPT_ENUM',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_PARAM', m4_defn(`M4_CONF_CMD_OPT_ENUM_PARAM_SHORT'))
        m4_include(M4_PROG_SYM()`_conf_schema.m4')
        m4_popdef(`M4_PARAM')
        m4_pushdef(`M4_FIRST', `true')
        m4_pushdef(`M4_PARAM', m4_defn(`M4_CONF_CMD_OPT_ENUM_PARAM_LONG'))
        m4_include(M4_PROG_SYM()`_conf_schema.m4')
        m4_popdef(`M4_PARAM')
        m4_popdef(`M4_FIRST')
    m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')


m4_dnl
m4_dnl Output a short option description character sequence.
m4_dnl See the *schema.m4 for parameters.
m4_dnl
m4_define(
    `M4_CONF_CMD_SHORTOPTS_PARAM',
    `
        m4_ifelse(
            m4_conf_origin_is_in_range(`opts', `$3'),
            1,
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

m4_dnl
m4_dnl Output a short option description string
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_SHORTOPTS',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_TYPE_INT', `:')
        m4_pushdef(`M4_TYPE_DOUBLE', `:')
        m4_pushdef(`M4_TYPE_STRING', `:')
        m4_pushdef(`M4_TYPE_STRING_ARRAY', `:')
        m4_pushdef(`M4_TYPE_BOOL', `::')
        m4_pushdef(`M4_TYPE_CHOICE', `:')
        m4_pushdef(`M4_PARAM', m4_defn(`M4_CONF_CMD_SHORTOPTS_PARAM'))
        m4_include(M4_PROG_SYM()`_conf_schema.m4')
        m4_popdef(`M4_PARAM')
        m4_popdef(`M4_TYPE_CHOICE')
        m4_popdef(`M4_TYPE_BOOL')
        m4_popdef(`M4_TYPE_STRING_ARRAY')
        m4_popdef(`M4_TYPE_STRING')
        m4_popdef(`M4_TYPE_DOUBLE')
        m4_popdef(`M4_TYPE_INT')
    m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')

m4_dnl
m4_dnl Output a long option description initializer.
m4_dnl See the *schema.m4 for parameters.
m4_dnl
m4_define(
    `M4_CONF_CMD_LONGOPTS_PARAM',
    `
        m4_ifelse(
            m4_conf_origin_is_in_range(`opts', `$3'),
            1,
            `
                m4_printl(
                   `        {',
                   `            .name = "m4_substr(m4_translit(`$1/$2', `/', `-'), 1)",')
                m4_print(
                   `            .val = OPT',
                   m4_translit(m4_translit(`$1/$2', `/', `_'), `a-z', `A-Z'))
                m4_printl(
                   `,',
                   `            .has_arg = $4,',
                   `        },')
            '
        )
    '
)

m4_dnl
m4_dnl Output a long option description array.
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_LONGOPTS',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_TYPE_INT',           `required_argument')
        m4_pushdef(`M4_TYPE_DOUBLE',        `required_argument')
        m4_pushdef(`M4_TYPE_STRING',        `required_argument')
        m4_pushdef(`M4_TYPE_STRING_ARRAY',  `required_argument')
        m4_pushdef(`M4_TYPE_BOOL',          `optional_argument')
        m4_pushdef(`M4_TYPE_CHOICE',        `required_argument')
        m4_pushdef(`M4_PARAM', m4_defn(`M4_CONF_CMD_LONGOPTS_PARAM'))
        m4_include(M4_PROG_SYM()`_conf_schema.m4')
        m4_popdef(`M4_PARAM')
        m4_popdef(`M4_TYPE_CHOICE')
        m4_popdef(`M4_TYPE_BOOL')
        m4_popdef(`M4_TYPE_STRING_ARRAY')
        m4_popdef(`M4_TYPE_STRING')
        m4_popdef(`M4_TYPE_DOUBLE')
        m4_popdef(`M4_TYPE_INT')
    m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')



m4_dnl
m4_dnl Output an option description.
m4_dnl  Arguments:
m4_dnl
m4_dnl      $1  Option signature
m4_dnl      $2  Option description
m4_dnl
m4_define(
    `M4_CONF_CMD_HELP_OPTS_CONTAINER_PARAM_OPT',
    `
        m4_print(
           `    "    ',
           `$1',
           m4_substr(`                            ', m4_len(`$1')),
           m4_cstr_esc(`$2'))
        m4_printl(`\n"')
    '
)

m4_dnl
m4_dnl Output option help string literal for a parameter.
m4_dnl
m4_define(
    `M4_CONF_CMD_HELP_OPTS_CONTAINER_PARAM',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    m4_conf_origin_is_in_range(`opts', `$3'),
                    1,
                    `
                        M4_CONF_CMD_HELP_OPTS_CONTAINER_PARAM_OPT(
                            m4_ifelse(`$6',,, `-$6`,' ')--m4_substr(m4_translit(`$1/$2', `/', `-'), 1)`$7',
                            `$8')
                    '
                )
            '
        )
    '
)

m4_dnl
m4_dnl Output option help string literal for a container.
m4_dnl
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_HELP_OPTS_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    M4_CONF_CONTAINER_SIZE(M4_PROG_SYM()`_conf_schema.m4',
                                           M4_PREFIX()`$2',
                                           `opts'),
                    0,
                    ,
                    `
                        m4_printl(
                           `    "\n"',
                           `    "$3 options:\n"')
                        m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')

                        m4_pushdef(`M4_CONTAINER', `')
                        m4_pushdef(`M4_PARAM', m4_defn(`M4_CONF_CMD_HELP_OPTS_CONTAINER_PARAM'))
                        m4_include(M4_PROG_SYM()`_conf_schema.m4')
                        m4_popdef(`M4_PARAM')
                        m4_popdef(`M4_CONTAINER')

                        m4_pushdef(`M4_PARAM', `')
                        m4_include(M4_PROG_SYM()`_conf_schema.m4')
                        m4_popdef(`M4_PARAM')

                        m4_popdef(`M4_PREFIX')
                    '
                )
            '
        )
    '
)

m4_dnl
m4_dnl Output option help string constant.
m4_dnl
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_HELP_OPTS',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_CONTAINER', m4_defn(`M4_CONF_CMD_HELP_OPTS_CONTAINER')), 
        m4_pushdef(`M4_PREFIX', `')
        M4_CONTAINER(`', `', `General')
        m4_popdef(`M4_PREFIX')
        m4_popdef(`M4_CONTAINER')
     m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_INT',
    `
        m4_printl(
           `            assert(optarg != NULL);',
           `            if (optarg == NULL) {',
           `                tlog_errs_pushf(perrs, "Option %s has no value", optname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            if (sscanf(optarg, "%" SCNd64 " %n", &val_int, &end) < 1 ||',
           `                optarg[end] != 0 || val_int < $2) {',
           `                tlog_errs_pushf(perrs, "Invalid %s option value: %s\n%s",',
           `                                optname, optarg, help);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            val = json_object_new_int64(val_int);')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_DOUBLE',
    `
        m4_printl(
           `            assert(optarg != NULL);',
           `            if (optarg == NULL) {',
           `                tlog_errs_pushf(perrs, "Option %s has no value", optname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            if (sscanf(optarg, "%lf %n", &val_double, &end) < 1 ||',
           `                optarg[end] != 0 || val_double < $2) {',
           `                tlog_errs_pushf(perrs, "Invalid %s option value: %s\n%s",',
           `                                optname, optarg, help);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            val = json_object_new_double(val_double);')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_STRING',
    `
        m4_printl(
           `            assert(optarg != NULL);',
           `            if (optarg == NULL) {',
           `                tlog_errs_pushf(perrs, "Option %s has no value", optname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            val = json_object_new_string(optarg);')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_STRING_ARRAY',
    `
        m4_printl(
           `            assert(optarg != NULL);',
           `            if (optarg == NULL) {',
           `                tlog_errs_pushf(perrs, "Option %s has no value", optname);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }',
           `            entry_val = json_object_new_string(optarg);',
           `            if (entry_val == NULL) {',
           `                grc = TLOG_GRC_ERRNO;',
           `                tlog_errs_pushc(perrs, grc);',
           `                tlog_errs_pushf(perrs, "Failed creating %s option value", optname);',
           `                goto cleanup;',
           `            }',
           `            if (prev_val == NULL) {',
           `                val = json_object_new_array();',
           `                if (val == NULL) {',
           `                    grc = TLOG_GRC_ERRNO;',
           `                    tlog_errs_pushc(perrs, grc);',
           `                    tlog_errs_pushf(perrs, "Failed creating %s option value", optname);',
           `                    goto cleanup;',
           `                }',
           `            } else {',
           `                val = prev_val;',
           `            }',
           `            if (json_object_array_add(val, entry_val) != 0) {',
           `                grc = TLOG_GRC_ERRNO;',
           `                tlog_errs_pushc(perrs, grc);',
           `                tlog_errs_pushf(perrs, "Failed adding %s option value entry", optname);',
           `                goto cleanup;',
           `            }',
           `            entry_val = NULL;')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_BOOL',
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
           `                tlog_errs_pushf(perrs,',
           `                                "Invalid %s option value: %s,\n"',
           `                                "expecting yes/on/true or no/off/false\n%s",',
           `                                optname, optarg, help);',
           `                grc = TLOG_RC_FAILURE;',
           `                goto cleanup;',
           `            }')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_CHOICE_LIST',
    `
        m4_ifelse(`$#', `0', `',
                  `$#', `1', `m4_print(`"$1"')',
                  `
                    m4_printl(`"$1"`,'')
                    m4_print(`                                      ')
                    M4_CONF_CMD_LOAD_ARGS_TYPE_CHOICE_LIST(m4_shift($@))
                  '
        )
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_TYPE_CHOICE',
    `
        m4_printl(
           `            {')
        m4_print(
           `                const char *list[] = {')
        M4_CONF_CMD_LOAD_ARGS_TYPE_CHOICE_LIST(m4_shift($@))
        m4_printl(
           `};',
           `                size_t i;',
           `                assert(optarg != NULL);',
           `                if (optarg == NULL) {',
           `                    tlog_errs_pushf(perrs, "Option %s has no value", optname);',
           `                    grc = TLOG_RC_FAILURE;',
           `                    goto cleanup;',
           `                }',
           `                for (i = 0;',
           `                     i < TLOG_ARRAY_SIZE(list) && strcmp(optarg, list[i]) != 0;',
           `                     i++);',
           `                if (i >= TLOG_ARRAY_SIZE(list)) {',
           `                    tlog_errs_pushf(perrs, "Invalid %s option value: %s\n%s",',
           `                                    optname, optarg, help);',
           `                    grc = TLOG_RC_FAILURE;',
           `                    goto cleanup;',
           `                }',
           `            }',
           `            val = json_object_new_string(optarg);')
    '
)

m4_define(
    `M4_CONF_CMD_LOAD_ARGS_PARAM',
    `
        m4_ifelse(
            m4_conf_origin_is_in_range(`opts', `$3'),
            1,
            `
                m4_print(
                   `        case OPT',
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
                m4_printl(
                   `            grc = tlog_json_object_object_get_path(conf, optpath, &prev_val, NULL);',
                   `            if (grc != TLOG_RC_OK) {',
                   `                tlog_errs_pushc(perrs, grc);',
                   `                tlog_errs_pushf(perrs,',
                   `                                "Failed retrieving %s option value", optname);',
                   `                goto cleanup;',
                   `            }')
                $4
                m4_printl(
                   `            break;',
                   `')
            '
        )
    '
)

m4_dnl
m4_dnl Output a command-line argument parsing function.
m4_dnl
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_CONF_CMD_LOAD_ARGS',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_printl(
           `static tlog_grc',
           `tlog_'M4_PROG_SYM()`_conf_cmd_load_args(struct tlog_errs **perrs,',
           `                            struct json_object *conf,',
           `                            const char *help,',
           `                            int argc, char **argv)',
           `{',
           `    /* Option codes */',
           `    enum opt {')
        M4_CONF_CMD_OPT_ENUM()
        m4_printl(
           `    };',
           `',
           `    /* Description of short options */')
        m4_print(
           `    static const char *shortopts = ":')
        M4_CONF_CMD_SHORTOPTS()
        m4_printl(
           `";',
           `',
           `    /* Description of long options */',
           `    static const struct option longopts[] = {')
        M4_CONF_CMD_LONGOPTS()
        m4_printl(
           `        {',
           `            .name = NULL',
           `        }',
           `    };',
           `',
           `    tlog_grc grc;',
           `    int opterr_orig;',
           `    int optind_orig;',
           `    int optcode;',
           `    const char *optname;',
           `    const char *optpath;',
           `    int64_t val_int;',
           `    double val_double;',
           `    struct json_object *prev_val;',
           `    struct json_object *val = NULL;',
           `    struct json_object *entry_val = NULL;',
           `    struct json_object *args = NULL;',
           `    int end;',
           `    int i;',
           `',
           `    /* May be unused if there are no corresponding parameters */',
           `    (void)val_int;',
           `    (void)val_double;',
           `    (void)end;',
           `',
           `    /* Ask getopt_long to not print an error message */',
           `    opterr_orig = opterr;',
           `    opterr = 0;',
           `',
           `    /* Ask getopt_long to start from the first argument */',
           `    optind_orig = optind;',
           `    optind = 1;',
           `',
           `    /* While there are options */',
           `    while ((optcode = getopt_long(argc, argv, ',
           `                                  shortopts, longopts, NULL)) >= 0) {',
           `        switch (optcode) {')
        m4_pushdef(`M4_TYPE_INT',           m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_INT'))
        m4_pushdef(`M4_TYPE_DOUBLE',        m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_DOUBLE'))
        m4_pushdef(`M4_TYPE_STRING',        m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_STRING'))
        m4_pushdef(`M4_TYPE_STRING_ARRAY',  m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_STRING_ARRAY'))
        m4_pushdef(`M4_TYPE_BOOL',          m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_BOOL'))
        m4_pushdef(`M4_TYPE_CHOICE',        m4_defn(`M4_CONF_CMD_LOAD_ARGS_TYPE_CHOICE'))
        m4_pushdef(`M4_PARAM',              m4_defn(`M4_CONF_CMD_LOAD_ARGS_PARAM'))
        m4_include(M4_PROG_SYM()`_conf_schema.m4')
        m4_popdef(`M4_PARAM')
        m4_popdef(`M4_TYPE_CHOICE')
        m4_popdef(`M4_TYPE_BOOL')
        m4_popdef(`M4_TYPE_STRING_ARRAY')
        m4_popdef(`M4_TYPE_STRING')
        m4_popdef(`M4_TYPE_DOUBLE')
        m4_popdef(`M4_TYPE_INT')
        m4_printl(
           `        case m4_singlequote(`:'):',
           `            for (i = 0; i < (int)TLOG_ARRAY_SIZE(longopts); i++) {',
           `                if (longopts[i].val == optopt) {',
           `                    break;',
           `                }',
           `            }',
           `            if (i < (int)TLOG_ARRAY_SIZE(longopts)) {',
           `                if (longopts[i].val < 0x100) {',
           `                    tlog_errs_pushf(perrs,',
           `                                    "Option -%c/--%s argument is missing\n%s",',
           `                                    longopts[i].val, longopts[i].name, help);',
           `                } else {',
           `                    tlog_errs_pushf(perrs,',
           `                                    "Option --%s argument is missing\n%s",',
           `                                    longopts[i].name, help);',
           `                }',
           `            } else {',
           `                tlog_errs_pushf(perrs,',
           `                                "Option -%c argument is missing\n%s",',
           `                                optopt, help);',
           `            }',
           `            grc = TLOG_RC_FAILURE;',
           `            goto cleanup;',
           `',
           `        case m4_singlequote(`?'):',
           `            if (optopt == 0) {',
           `                tlog_errs_pushf(perrs,',
           `                                "Unknown option encountered\n%s",',
           `                                help);',
           `            } else {',
           `                tlog_errs_pushf(perrs,',
           `                                "Unknown option encountered: -%c\n%s",',
           `                                optopt, help);',
           `            }',
           `            grc = TLOG_RC_FAILURE;',
           `            goto cleanup;',
           `',
           `        default:',
           `            tlog_errs_pushf(perrs, "Unknown option code: %d", optcode);',
           `            grc = TLOG_RC_FAILURE;',
           `            goto cleanup;',
           `        }',
           `',
           `        if (val == NULL) {',
           `            grc = TLOG_GRC_ERRNO;',
           `            tlog_errs_pushc(perrs, grc);',
           `            tlog_errs_pushf(perrs, "Failed creating %s option value", optname);',
           `            goto cleanup;',
           `        }',
           `        if (val != prev_val) {',
           `            grc = tlog_json_object_object_add_path(conf, optpath, val);',
           `            if (grc != TLOG_RC_OK) {',
           `                tlog_errs_pushc(perrs, grc);',
           `                tlog_errs_pushf(perrs, "Failed storing %s option value", optname);',
           `                goto cleanup;',
           `            }',
           `        }',
           `        val = NULL;',
           `    }',
           `',
           `    /* Add other arguments */',
           `    args = json_object_new_array();',
           `    if (args == NULL) {',
           `        grc = TLOG_GRC_ERRNO;',
           `        tlog_errs_pushc(perrs, grc);',
           `        tlog_errs_pushs(perrs, "Failed creating positional argument list");',
           `        goto cleanup;',
           `    }',
           `    for (i = 0; optind < argc; i++, optind++) {',
           `        val = json_object_new_string(argv[optind]);',
           `        if (val == NULL) {',
           `            grc = TLOG_GRC_ERRNO;',
           `            tlog_errs_pushc(perrs, grc);',
           `            tlog_errs_pushs(perrs, "Failed creating argument value");',
           `            goto cleanup;',
           `        }',
           `        if (json_object_array_put_idx(args, i, val) < 0) {',
           `            grc = TLOG_GRC_ERRNO;',
           `            tlog_errs_pushc(perrs, grc);',
           `            tlog_errs_pushf(perrs, "Failed storing argument #%d", i);',
           `            goto cleanup;',
           `        }',
           `        val = NULL;',
           `    }',
           `    grc = tlog_json_object_object_add_path(conf, "args", args);',
           `    if (grc != TLOG_RC_OK) {',
           `        tlog_errs_pushc(perrs, grc);',
           `        tlog_errs_pushs(perrs, "Failed storing argument list");',
           `        goto cleanup;',
           `    }',
           `    args = NULL;',
           `',
           `    /* Validate the result */',
           `    grc = tlog_'M4_PROG_SYM()`_conf_validate(perrs, conf, TLOG_CONF_ORIGIN_ARGS);',
           `    if (grc != TLOG_RC_OK) {',
           `        tlog_errs_pushs(perrs, "Failed to validate loaded configuration");',
           `        goto cleanup;',
           `    }',
           `',
           `    grc = TLOG_RC_OK;',
           `',
           `cleanup:',
           `    json_object_put(args);',
           `    json_object_put(entry_val);',
           `    json_object_put(val);',
           `    optind = optind_orig;',
           `    opterr = opterr_orig;',
           `    return grc;',
           `}')
     m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')
m4_divert(0)m4_dnl
