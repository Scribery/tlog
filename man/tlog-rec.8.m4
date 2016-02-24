m4_include(`misc.m4')m4_dnl
.\" Process this file with
.\" groff -man -Tascii tlog-rec.8
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
.TH tlog-rec "8" "February 2016" "Linux"
.SH NAME
tlog-rec \- start a shell and log terminal I/O

.SH SYNOPSIS
.B tlog-rec
[OPTION...] [CMD_FILE [CMD_ARG...]]
.br
.B tlog-rec
-c [OPTION...] CMD_STRING [CMD_NAME [CMD_ARG...]]

.SH DESCRIPTION
.B Tlog-rec
is a terminal I/O logging program. It starts a shell under a pseudo-TTY,
connects it to the actual terminal and logs whatever passes between them
including user input, program output, and terminal window size changes.

If no "-c" option is specified, then the first non-option argument CMD_FILE
specifies the location of a shell script the shell should read and the
following arguments (CMD_ARG) specify its arguments.

If the "-c" option is specified, then a non-option argument CMD_STRING is
required and should contain shell commands to execute, the following
arguments can specify first the script name (CMD_NAME, i.e. argv[0]) and then
its arguments (CMD_ARG).

If no non-option arguments are encountered, then the shell is started
interactively.

.SH OPTIONS
m4_divert(-1)
m4_define(`M4_PREFIX', `')

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
                        m4_printl(`.TP')
                        m4_print(
                            `.B ',
                            m4_ifelse(`$6',,, `-$6`,' '),
                            `--',
                            m4_substr(m4_translit(`$1/$2', `/', `-'), 1),
                            `$7')
                        m4_printl(
                            `',
                            `$8')
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
                m4_printl(`.SS $3 options')
                m4_pushdef(`M4_PREFIX', M4_PREFIX()`$2')

                m4_pushdef(`M4_CONTAINER', `')
                m4_pushdef(`M4_PARAM', m4_defn(`M4_CONTAINER_PARAM'))
                m4_include(`rec_conf_schema.m4')
                m4_popdef(`M4_PARAM')
                m4_popdef(`M4_CONTAINER')

                m4_include(`rec_conf_schema.m4')

                m4_popdef(`M4_PREFIX')
            '
        )
    '
)

M4_CONTAINER(`', `', `General')

m4_divert(0)
.SH ENVIRONMENT
.TP
TLOG_REC_CONF_FILE
Specifies the location of a configuration file to be read.
The configuration parameters in this file override the ones in the systemwide
configuration file M4_CONF_PATH().

.TP
TLOG_REC_CONF_TEXT
Specifies the configuration text to be read.
The configuration parameters in this variable override the ones in the file
specified with TLOG_REC_CONF_FILE.

.SH FILES
.TP
M4_CONF_PATH()
The systemwide configuration file

.SH EXAMPLES
.TP
Start recording a login shell:
.B tlog-rec -l

.TP
Start recording a zsh session:
.B tlog-rec -s /usr/bin/zsh

.TP
Record everything but user input:
.B tlog-rec --log-input=off --log-output=on --log-window=on

.TP
Ask the recorded shell to execute a command:
.B tlog-rec -c whoami

.SH SEE ALSO
tlog-rec.conf(5)

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
