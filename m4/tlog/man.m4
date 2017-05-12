m4_dnl
m4_dnl Tlog m4 macros for generating manpages.
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
m4_dnl
m4_divert(-1)
m4_define(
    `M4_MAN_CONF_TYPE_INT',
    `
        m4_ifelse(
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`integer')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`$1')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `desc',
            `
                m4_ifelse(
                    `$2',
                    `',
                    ,
                    `
                        m4_printl(`Minimum: $2', `.br')
                    '
                )
            ',
        )
    '
)

m4_define(
    `M4_MAN_CONF_TYPE_STRING',
    `
        m4_ifelse(
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`string')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
        )
    '
)

m4_define(
    `M4_MAN_CONF_TYPE_BOOL',
    `
        m4_ifelse(
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`boolean')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`$1')',
        )
    '
)

m4_define(
    `M4_MAN_CONF_TYPE_CHOICE_LIST',
    `
        m4_ifelse(
            `$#', `0', `',
            `$#', `1', `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
            `
                m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"`, '')
                M4_MAN_CONF_TYPE_CHOICE_LIST(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_MAN_CONF_TYPE_CHOICE',
    `
        m4_ifelse(
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`string')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
            M4_MAN_CONF_TYPE_EXPAND_TO(),
            `desc',
            `
                m4_ifelse(
                    `$2',
                    `',
                    ,
                    `
                        m4_print(`One of: ')
                        M4_MAN_CONF_TYPE_CHOICE_LIST(m4_shift($@))
                        m4_printl(`', `.br')
                    '
                )
            ',
        )
    '
)

m4_define(
    `M4_MAN_CONF_LINES',
    `
        m4_ifelse(
            `$#', `0',
            `',
            `$#', `1',
            `
                m4_printl(`$1')
            ',
            `
                m4_printl(`$1')
                M4_MAN_CONF_LINES(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_MAN_CONF_PARAM_DESC',
    `
        m4_ifelse(
            `$1',
            M4_MAN_CONF_PREFIX(),
            `
                m4_ifelse(
                    m4_conf_origin_is_in_range(`file', `$3'),
                    1,
                    `
                        m4_pushdef(`M4_MAN_CONF_TYPE_EXPAND_TO', `sig')
                        m4_printl(`.TP')
                        m4_print(`$2 (')
                        $4
                        m4_printl(`)')
                        $9
                        m4_popdef(`M4_MAN_CONF_TYPE_EXPAND_TO')
                        m4_printl(`')
                        m4_pushdef(`M4_MAN_CONF_TYPE_EXPAND_TO', `desc')
                        $4
                        m4_popdef(`M4_MAN_CONF_TYPE_EXPAND_TO')
                        m4_ifelse(
                            `$5',
                            `true',
                            `
                                m4_pushdef(`M4_MAN_CONF_TYPE_EXPAND_TO', `def')
                                m4_print(`Default: ')
                                $4
                                m4_printl(`')
                                m4_popdef(`M4_MAN_CONF_TYPE_EXPAND_TO')
                            ',
                            `
                                m4_printl(`No default.')
                            '
                        )
                    '
                )
            '
        )
    '
)

m4_define(
    `M4_MAN_CONF_CONTAINER_DESC',
    `
        m4_ifelse(
            `$1',
            M4_MAN_CONF_PREFIX(),
            `
                m4_printl(
                    `.TP',
                    m4_substr(`$2', 1)` (object)',
                    `$3 object, see below.',
                    `.br')
            '
        )
    '
)

m4_define(
    `M4_MAN_CONF_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_MAN_CONF_PREFIX(),
            `
                m4_print(
                    `.SS ',
                    `m4_ifelse(
                        `$2',
                        `',
                        ,
                        `m4_substr(m4_translit(`$1$2', `/', `.'), 1) - ')',
                    `$3 object')
                m4_printl(`')
                m4_pushdef(`M4_MAN_CONF_PREFIX', M4_MAN_CONF_PREFIX()`$2')

                m4_pushdef(`M4_CONTAINER', m4_defn(`M4_MAN_CONF_CONTAINER_DESC'))
                m4_pushdef(`M4_PARAM', m4_defn(`M4_MAN_CONF_PARAM_DESC'))
                m4_include(M4_PROG_SYM()`_conf_schema.m4')
                m4_popdef(`M4_PARAM')
                m4_popdef(`M4_CONTAINER')

                m4_include(M4_PROG_SYM()`_conf_schema.m4')

                m4_popdef(`M4_MAN_CONF_PREFIX')
            '
        )
    '
)

m4_dnl
m4_dnl Output a description of configuration nodes.
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_MAN_CONF',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_MAN_CONF_PREFIX', `')
        m4_pushdef(`M4_MAN_CONF_TYPE_EXPAND_TO', `')
        m4_pushdef(`M4_TYPE_INT',       m4_defn(`M4_MAN_CONF_TYPE_INT'))
        m4_pushdef(`M4_TYPE_STRING',    m4_defn(`M4_MAN_CONF_TYPE_STRING'))
        m4_pushdef(`M4_TYPE_BOOL',      m4_defn(`M4_MAN_CONF_TYPE_BOOL'))
        m4_pushdef(`M4_TYPE_CHOICE',    m4_defn(`M4_MAN_CONF_TYPE_CHOICE'))
        m4_pushdef(`M4_LINES',          m4_defn(`M4_MAN_CONF_LINES'))
        m4_pushdef(`M4_CONTAINER',      m4_defn(`M4_MAN_CONF_CONTAINER'))
        M4_CONTAINER(`', `', `Root')
        m4_popdef(`M4_CONTAINER')
        m4_popdef(`M4_LINES')
        m4_popdef(`M4_TYPE_CHOICE')
        m4_popdef(`M4_TYPE_BOOL')
        m4_popdef(`M4_TYPE_STRING')
        m4_popdef(`M4_TYPE_INT')
        m4_popdef(`M4_MAN_CONF_TYPE_EXPAND_TO')
        m4_popdef(`M4_MAN_CONF_PREFIX')
     m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')

m4_define(
    `M4_MAN_OPTS_TYPE_INT',
    `
        m4_ifelse(
            `$2',
            `',
            ,
            `
                m4_printl(`Value minimum: $2', `.br')
            '
        )
    '
)

m4_define(`M4_MAN_OPTS_TYPE_STRING', `')

m4_define(`M4_MAN_OPTS_TYPE_BOOL', `')

m4_define(
    `M4_MAN_OPTS_TYPE_CHOICE_LIST',
    `
        m4_ifelse(
            `$#', `0', `',
            `$#', `1', `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
            `
                m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"`, '')
                M4_MAN_OPTS_TYPE_CHOICE_LIST(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_MAN_OPTS_TYPE_CHOICE',
    `
        m4_ifelse(
            `$2',
            `',
            ,
            `
                m4_print(`Value should be one of: ')
                M4_MAN_OPTS_TYPE_CHOICE_LIST(m4_shift($@))
                m4_printl(`', `.br')
            '
        )
    '
)

m4_dnl
m4_dnl Output a description of a configuration parameter
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_MAN_OPTS_CONTAINER_PARAM',
    `
        m4_ifelse(
            `$1',
            M4_MAN_OPTS_PREFIX(),
            `
                m4_ifelse(
                    m4_conf_origin_is_in_range(`opts', `$3'),
                    1,
                    `
                        m4_printl(`.TP')
                        m4_print(
                            `.B ',
                            m4_ifelse(`$6',,, `-$6`,' '),
                            `--',
                            m4_substr(m4_translit(`$1/$2', `/', `-'), 1),
                            `$7')
                        m4_printl(
                            `',
                            `$8',
                            `')
                        $4
                    '
                )
            '
        )
    '
)

m4_dnl
m4_dnl Increment the global macro M4_MAN_OPTS_CONTAINER_SIZE_VAL, if a
m4_dnl parameter belongs to the current container specified with
m4_dnl M4_MAN_OPTS_PREFIX and is an option.
m4_dnl
m4_dnl Macros:
m4_dnl
m4_dnl  M4_MAN_OPTS_PREFIX - container prefix (`' for root)
m4_dnl  M4_MAN_OPTS_CONTAINER_SIZE_VAL - the macro to increment
m4_dnl
m4_define(
`M4_MAN_OPTS_CONTAINER_SIZE_ADD_PARAM',
`m4_ifelse(`$1', M4_MAN_OPTS_PREFIX(),
`m4_ifelse(m4_conf_origin_is_in_range(`opts', `$3'), 1,
`m4_define(`M4_MAN_OPTS_CONTAINER_SIZE_VAL',
m4_incr(M4_MAN_OPTS_CONTAINER_SIZE_VAL)m4_dnl
)'m4_dnl
)'m4_dnl
)'m4_dnl
)

m4_dnl
m4_dnl Expand to the number of parameter options to be output for a parameter
m4_dnl container.
m4_dnl Arguments:
m4_dnl
m4_dnl  $1 Name of a container in M4_MAN_OPTS_PREFIX to calculate the size of
m4_dnl
m4_dnl Macros:
m4_dnl
m4_dnl  M4_MAN_OPTS_PREFIX - container prefix (`' for root)
m4_dnl
m4_define(
`M4_MAN_OPTS_CONTAINER_SIZE',
`m4_pushdef(`M4_MAN_OPTS_CONTAINER_SIZE_VAL', `0')m4_dnl
m4_pushdef(`M4_CONTAINER', `')m4_dnl
m4_pushdef(`M4_PARAM', m4_defn(`M4_MAN_OPTS_CONTAINER_SIZE_ADD_PARAM'))m4_dnl
m4_pushdef(`M4_MAN_OPTS_PREFIX', M4_MAN_OPTS_PREFIX()`$1')m4_dnl
m4_include(M4_PROG_SYM()`_conf_schema.m4')m4_dnl
m4_popdef(`M4_MAN_OPTS_PREFIX')m4_dnl
m4_popdef(`M4_PARAM')m4_dnl
m4_popdef(`M4_CONTAINER')m4_dnl
M4_MAN_OPTS_CONTAINER_SIZE_VAL()m4_dnl
m4_popdef(`M4_MAN_OPTS_CONTAINER_SIZE_VAL')'m4_dnl
)

m4_define(
    `M4_MAN_OPTS_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_MAN_OPTS_PREFIX(),
            `
                m4_ifelse(
                    M4_MAN_OPTS_CONTAINER_SIZE(`$2'),
                    0,
                    ,
                    `
                        m4_printl(`.SS $3 options')
                        m4_pushdef(`M4_MAN_OPTS_PREFIX', M4_MAN_OPTS_PREFIX()`$2')

                        m4_pushdef(`M4_CONTAINER', `')
                        m4_pushdef(`M4_PARAM', m4_defn(`M4_MAN_OPTS_CONTAINER_PARAM'))
                        m4_include(M4_PROG_SYM()`_conf_schema.m4')
                        m4_popdef(`M4_PARAM')
                        m4_popdef(`M4_CONTAINER')

                        m4_include(M4_PROG_SYM()`_conf_schema.m4')

                        m4_popdef(`M4_MAN_OPTS_PREFIX')
                    '
                )
            '
        )
    '
)

m4_dnl
m4_dnl Output a description of command-line options
m4_dnl Macros:
m4_dnl
m4_dnl  M4_PROG_SYM - program-identifying symbol fragment.
m4_dnl
m4_define(
    `M4_MAN_OPTS',
    `m4_pushdef(`m4_orig_divnum', m4_divnum)m4_divert(-1)
        m4_pushdef(`M4_MAN_OPTS_PREFIX', `')
        m4_pushdef(`M4_MAN_OPTS_TYPE_EXPAND_TO', `')
        m4_pushdef(`M4_TYPE_INT',       m4_defn(`M4_MAN_OPTS_TYPE_INT'))
        m4_pushdef(`M4_TYPE_STRING',    m4_defn(`M4_MAN_OPTS_TYPE_STRING'))
        m4_pushdef(`M4_TYPE_BOOL',      m4_defn(`M4_MAN_OPTS_TYPE_BOOL'))
        m4_pushdef(`M4_TYPE_CHOICE',    m4_defn(`M4_MAN_OPTS_TYPE_CHOICE'))
        m4_pushdef(`M4_CONTAINER', m4_defn(`M4_MAN_OPTS_CONTAINER'))
        M4_CONTAINER(`', `', `General')
        m4_popdef(`M4_TYPE_CHOICE')
        m4_popdef(`M4_TYPE_BOOL')
        m4_popdef(`M4_TYPE_STRING')
        m4_popdef(`M4_TYPE_INT')
        m4_popdef(`M4_MAN_OPTS_TYPE_EXPAND_TO')
        m4_popdef(`M4_MAN_OPTS_PREFIX')
     m4_divert(m4_orig_divnum)m4_popdef(`m4_orig_divnum')')

m4_divert(0)m4_dnl
