m4_include(`man.m4')m4_dnl
.\" Process this file with
.\" groff -man -Tascii
m4_generated_warning(`.\" ')m4_dnl
.\"
.\" Copyright (C) 2016-2017 Red Hat
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
.TH tlog-M4_PROG_NAME() "8" "May 2017" "Tlog"
.SH NAME
tlog-M4_PROG_NAME() \- record terminal I/O of a program or a user shell

.SH SYNOPSIS
.B tlog-rec
[OPTION...] [CMD_FILE [CMD_ARG...]]

.SH DESCRIPTION
.B Tlog-rec
is a terminal I/O logging program. It starts a program under a pseudo-TTY,
connects it to the actual terminal and logs whatever passes between them
including user input, program output, and terminal window size changes.

CMD_FILE argument specifies the program to run and record. If CMD_FILE
contains a slash (/) character, then it is assumed to contain a path to the
program to run. Otherwise a program file with CMD_FILE name is searched for in
directories specified with the PATH environment variable. If this variable is
not set, then the current directory is searched, followed by the
system-default directories output by "getconf CS_PATH", which is usually
"/bin:/usr/bin".

CMD_ARG arguments are used as arguments to the program to run and record.

If no non-option arguments are specified, then tlog-rec starts and records a
user shell specified with the SHELL environment variable, or if that is not
set, it starts the shell specified in the NSS database for the user tlog-rec
runs as.

.B Tlog-rec
loads its parameters first from the system-wide configuration file
M4_CONF_PATH(), then from the file pointed at by TLOG_REC_CONF_FILE
environment variable (if set), then from the contents of the TLOG_REC_CONF_TEXT
environment variable (if set), and then from command-line options. Parameters
from each of these sources override the previous one in turn.

.SH OPTIONS
M4_MAN_OPTS()

.SH ENVIRONMENT
.TP
TLOG_REC_CONF_FILE
Specifies the location of a configuration file to be read.
The configuration parameters in this file override the ones in the system-wide
configuration file M4_CONF_PATH().

.TP
TLOG_REC_CONF_TEXT
Specifies the configuration text to be read.
The configuration parameters in this variable override the ones in the file
specified with TLOG_REC_CONF_FILE.

.TP
SHELL
Specifies the shell to run, if no positional arguments are found on the
command line.

.SH FILES
.TP
M4_CONF_PATH()
The system-wide configuration file

.SH EXAMPLES
.TP
Record a vim session to a file:
.B tlog-rec -o vim.log vim

.TP
Record user input only:
.B tlog-rec --log-input=on --log-output=off --log-window=off

.TP
Record with minimal latency:
.B tlog-rec --latency=1

.SH SEE ALSO
tlog-M4_PROG_NAME().conf(5), tlog-rec-session(8), tlog-play(8)

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
