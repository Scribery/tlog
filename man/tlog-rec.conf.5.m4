m4_include(`misc.m4')m4_dnl
.\" Process this file with
.\" groff -man -Tascii tlog-rec.conf.5
.\"
m4_generated_warning(`.\" ')m4_dnl
.\"
.\" Copyright (C) 2016 Red Hat
.\"
.\" This file is part of tlog.
.\"
.\" Tlog is free software; you can redistribute it and/or modify
.\" it under the terms of the GNU General Public License as published by
.\" the Free Software Foundation; either version 2 of the License, or
.\" (at your option) any later version.
.\"
.\" Tlog is distributed in the hope that it will be useful,
.\" but WITHOUT ANY WARRANTY; without even the implied warranty of
.\" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
.\" GNU General Public License for more details.
.\"
.\" You should have received a copy of the GNU General Public License
.\" along with tlog; if not, write to the Free Software
.\" Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
.\"
.TH tlog-rec "5" "February 2016" "Linux"
.SH NAME
tlog-rec.conf \- tlog-rec configuration file

.SH DESCRIPTION
.B tlog-rec.conf
is a JSON-format configuration file for
.B tlog-rec
program.
Contrary to the strict JSON specification, both C and C++ style comments are
allowed in the file.

The file must contain a single JSON object with the objects and fields
described below.
Almost all of them are optional and assume a default value.
However, those that do require a value can still be omitted and specified to
.B tlog-rec
in other ways: through environment variables or command line.

.SH OBJECTS AND FIELDS
m4_divert(-1)
m4_define(`M4_PREFIX', `')
m4_define(`M4_TYPE_EXPAND_TO', `')

m4_define(
    `M4_TYPE_INT',
    `
        m4_ifelse(
            M4_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`integer')',
            M4_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`$1')',
            M4_TYPE_EXPAND_TO(),
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
    `M4_TYPE_STRING',
    `
        m4_ifelse(
            M4_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`string')',
            M4_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
        )
    '
)

m4_define(
    `M4_TYPE_BOOL',
    `
        m4_ifelse(
            M4_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`boolean')',
            M4_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`$1')',
        )
    '
)

m4_define(
    `M4_TYPE_CHOICE_LIST',
    `
        m4_ifelse(
            `$#', `0', `',
            `$#', `1', `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
            `
                m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"`, '')
                M4_TYPE_CHOICE_LIST(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_TYPE_CHOICE',
    `
        m4_ifelse(
            M4_TYPE_EXPAND_TO(),
            `sig',
            `m4_print(`string')',
            M4_TYPE_EXPAND_TO(),
            `def',
            `m4_print(`"'m4_patsubst(`$1', `\\', `\\\\')`"')',
            M4_TYPE_EXPAND_TO(),
            `desc',
            `
                m4_ifelse(
                    `$2',
                    `',
                    ,
                    `
                        m4_print(`One of: ')
                        M4_TYPE_CHOICE_LIST(m4_shift($@))
                        m4_printl(`', `.br')
                    '
                )
            ',
        )
    '
)

m4_define(
    `M4_LINES',
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
                M4_LINES(m4_shift($@))
            '
        )
    '
)

m4_define(
    `M4_PARAM_DESC',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
            `
                m4_ifelse(
                    `$3',
                    `file',
                    `
                        m4_pushdef(`M4_TYPE_EXPAND_TO', `sig')
                        m4_printl(`.TP')
                        m4_print(`$2 (')
                        $4
                        m4_printl(`)')
                        $9
                        m4_popdef(`M4_TYPE_EXPAND_TO')
                        m4_printl(`.sp 1')
                        m4_pushdef(`M4_TYPE_EXPAND_TO', `desc')
                        $4
                        m4_popdef(`M4_TYPE_EXPAND_TO')
                        m4_ifelse(
                            `$5',
                            `true',
                            `
                                m4_pushdef(`M4_TYPE_EXPAND_TO', `def')
                                m4_print(`Default: ')
                                $4
                                m4_printl(`')
                                m4_popdef(`M4_TYPE_EXPAND_TO')
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
    `M4_CONTAINER_DESC',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
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
    `M4_CONTAINER',
    `
        m4_ifelse(
            `$1',
            M4_PREFIX(),
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
                m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')

                m4_pushdef(`M4_CONTAINER', m4_defn(`M4_CONTAINER_DESC'))
                m4_pushdef(`M4_PARAM', m4_defn(`M4_PARAM_DESC'))
                m4_include(`rec_conf_schema.m4')
                m4_popdef(`M4_PARAM')
                m4_popdef(`M4_CONTAINER')

                m4_include(`rec_conf_schema.m4')

                m4_popdef(`M4_PREFIX')
            '
        )
    '
)

M4_CONTAINER(`', `', `Root')

m4_divert(0)
.SH EXAMPLES
.TP
A config specifying only a shell:
.nf

{
    "shell": "/usr/bin/zsh"
}
.fi

.TP
A config disabling logging user input:
.nf

{
    "log": {
        "input": false
    }
}
.fi

.TP
A config specifying logging to a file:
.nf

{
    "writer": "file"
    "file" : {
        "path": "/var/log/tlog-rec.log"
    }
}
.fi

.SH SEE ALSO
tlog-rec(8), http://json.org/

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
