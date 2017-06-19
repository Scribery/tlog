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
tlog-rec-session \- start a shell and log terminal I/O

.SH SYNOPSIS
.B tlog-rec-session
[OPTION...] [CMD_FILE [CMD_ARG...]]
.br
.B tlog-rec-session
-c [OPTION...] CMD_STRING [CMD_NAME [CMD_ARG...]]

.SH DESCRIPTION
.B Tlog-rec-session
is a terminal session I/O logging program, intended for use as the login shell
for a user. The actual user shell to start is retrieved from configuration or
environment. Tlog-rec-session starts the actual shell under a pseudo-TTY,
connects it to the actual terminal and logs whatever passes between them
including user input, program output, and terminal window size changes.

Tlog-rec-session will not start recording if another process with the same
audit session ID (as seen in /proc/PID/sessionid) is already being recorded.
Instead, it will simply start the shell.

If no "-c" option is specified, then the first non-option argument CMD_FILE
specifies the location of a shell script the shell should read and the
following arguments (CMD_ARG) specify its arguments.

If the "-c" option is specified, then a non-option argument CMD_STRING is
required and should contain shell commands to execute, the following
arguments can specify first the script name (CMD_NAME, i.e. argv[0]) and then
its arguments (CMD_ARG).

If no non-option arguments are encountered, then the shell is started
interactively.

If tlog-rec-session is invoked under a name beginning with a dash (i.e.
argv[0] beginning with '-'), then the executed shell name is also prepended
with a dash. This passes the "login shell" status to the recorded shell and is
equivalent to using the "-l" option.

.B Tlog-rec-session
loads its parameters first from the system-wide configuration file
M4_CONF_PATH(), then from the file pointed at by TLOG_REC_SESSION_CONF_FILE
environment variable (if set), then from the contents of the
TLOG_REC_SESSION_CONF_TEXT environment variable (if set), and then from
command-line options. Parameters from each of these sources override the
previous one in turn.

.SH OPTIONS
M4_MAN_OPTS()

.SH ENVIRONMENT
.TP
TLOG_REC_SESSION_CONF_FILE
Specifies the location of a configuration file to be read.
The configuration parameters in this file override the ones in the system-wide
configuration file M4_CONF_PATH().

.TP
TLOG_REC_SESSION_CONF_TEXT
Specifies the configuration text to be read.
The configuration parameters in this variable override the ones in the file
specified with TLOG_REC_SESSION_CONF_FILE.

.TP
TLOG_REC_SESSION_SHELL
Specifies the shell to spawn. Overrides configuration specified with
TLOG_REC_SESSION_CONF_TEXT. Can be overridden with command-line options.

.SH FILES
.TP
M4_CONF_PATH()
The system-wide configuration file

.SH EXAMPLES
.TP
Start recording a login shell:
.B tlog-rec-session -l

.TP
Ask the recorded shell to execute a command:
.B tlog-rec-session -c whoami

.SH SEE ALSO
tlog-M4_PROG_NAME().conf(5), tlog-rec(8), tlog-play(8)

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
