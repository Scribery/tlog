m4_include(`misc.m4')m4_dnl
//
// Tlog-rec M4_CONF_TYPE() configuration. See tlog-rec.conf(5) for details.
// This file uses JSON format with both C and C++ comments allowed.
//
m4_generated_warning(`// ')m4_dnl
m4_divert(-1)

m4_define(`M4_PREFIX', `')
m4_define(`M4_INDENT', `')
m4_define(`M4_FIRST', `true')

m4_define(
    `M4_TYPE_INT',
    `m4_print(`$1')'
)

m4_define(
    `M4_TYPE_STRING',
    `m4_print(`"$1"')'
)

m4_define(
    `M4_TYPE_BOOL',
    `m4_print(`$1')'
)

m4_define(
    `M4_TYPE_CHOICE',
    `m4_print(`"$1"')'
)

m4_define(
    `M4_LINES',
    `
        m4_ifelse(
            `$#', `0',
            `',
            `$#', `1',
            `
                m4_printl(M4_INDENT()`// $1')
            ',
            `
                m4_printl(M4_INDENT()`// $1')
                M4_LINES(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_PARAM',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    `$3',
                    `file',
                    `
                        m4_ifelse(
                            M4_FIRST(),
                            `true',
                            `m4_define(`M4_FIRST', `false')',
                            `m4_printl(`,', `')'
                        )
                        $9
                        m4_print(M4_INDENT())
                        m4_ifelse(
                            `$5',
                            `false',
                            `m4_print(`// ')',
                            M4_COMMENT_OUT(),
                            `true',
                            `m4_print(`// ')'
                        )
                        m4_print(`"$2" : ')$4
                    '
                )
            '
        )
    '
)

m4_define(
    `M4_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    M4_FIRST(),
                    `true',
                    `m4_define(`M4_FIRST', `false')',
                    `m4_printl(`,', `')'
                )
                m4_ifelse(
                    `$2',
                    `',
                    `m4_printl(M4_INDENT()`{')',
                    `
                        m4_printl(M4_INDENT()`// $3 parameters')
                        m4_printl(M4_INDENT()`"m4_substr(`$2', 1)": {')
                    '
                )
                m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')
                m4_pushdef(`M4_INDENT', M4_INDENT()`    ')
                m4_pushdef(`M4_FIRST', `true')

                m4_include(`rec_conf_schema.m4')

                m4_popdef(`M4_FIRST')
                m4_popdef(`M4_INDENT')
                m4_popdef(`M4_PREFIX')
                m4_printl(`')
                m4_print(M4_INDENT()`}')
            '
        )
    '
)

M4_CONTAINER(`', `', `')
