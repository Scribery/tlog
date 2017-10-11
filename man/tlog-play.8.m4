m4_include(`man.m4')m4_dnl
.\" Process this file with
.\" groff -man -Tascii
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
.TH tlog-M4_PROG_NAME() "8" "March 2016" "Tlog"
.SH NAME
tlog-play \- play back terminal I/O recorded by tlog-rec(8)

.SH SYNOPSIS
.B tlog-play
[OPTION...]

.SH DESCRIPTION
.B Tlog-play
is a playback program for terminal I/O recorded with tlog-rec(8).  It
reproduces the recording on the terminal it's run under, and can't change its
size, so the playback terminal size needs to match the recorded terminal size
for proper playback.

.B Tlog-play
loads its parameters from the system-wide configuration file M4_CONF_PATH(),
which can be overridden with command-line options described below.

.SH OPTIONS
M4_MAN_OPTS()

.SH CONTROLS
Playback can be controlled using the following keys:

.TP
.B SPACE, p
Pause/resume playback.

.TP
.B }
Double the playback speed. Maximum is 16x.

.TP
.B {
Halve the playback speed. Minimum is 1/16x.

.TP
.B BACKSPACE
Reset playback to normal, 1x speed.

.TP
.B .
Output the next packet immediately, without delay, regardless if paused or
not. Press when paused to step through recording. Press once to skip a long
pause. Hold to skip through recording at constant speed (the keyboard repeat
rate).

.TP
.B G
Fast-forward the recording to the end, or to specified time. Works while
playing and on pause. The time can be specified by typing in a timestamp
before pressing 'G'. The timestamp should follow the format of the -g/--goto
option value, but without the fractions of a second. The command has no
effect, if the specified time location has already been reached.

E.g. pressing just 'G' would fast-forward to the end, which is useful with
following enabled. Pressing '3', '0', 'G' (typing "30G") would fast-forward to
30 seconds from the start of the recording. Typing "30:00G" would fast-forward
to 30 minutes, and so would "30:G", and "1800G". Typing "2::G" would
fast-forward to two hours into the recording, the same as "120:G" and "7200G".

.TP
.B q
Stop playing and quit.

.SH FILES
.TP
M4_CONF_PATH()
The system-wide configuration file

.SH EXAMPLES
.TP
Play back contents of a file written with tlog-rec's "file" writer:
.B tlog-play -r file --file-path=recording.log

.TP
Play back a recording from Journal:
.B tlog-play -r journal -M TLOG_REC=6071524bb44d403991a00413ab7c8596-53bd-378c5d9

.TP
Play back a recording from ElasticSearch:
.B tlog-play -r es --es-baseurl=http://localhost:9200/tlog/tlog/_search --es-query=session:121

.SH SEE ALSO
tlog-M4_PROG_NAME().conf(5), tlog-rec(8)

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
