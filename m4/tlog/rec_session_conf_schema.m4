m4_dnl
m4_dnl Tlog-rec-session configuration schema
m4_dnl
m4_dnl See conf_schema.m4 for macro invocations expected here.
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
m4_include(`misc.m4')m4_dnl
m4_include(`conf_schema.m4')m4_dnl
m4_dnl
M4_PARAM(`', `shell', `file-env',
         `M4_TYPE_STRING(`/bin/bash')', true,
         `s', `=SHELL', `Spawn the specified SHELL',
         `SHELL is the ', `The ',
         `M4_LINES(`path to the shell executable which should be spawned.')')m4_dnl
m4_dnl
M4_PARAM(`', `session_locking', `file-env',
         `M4_TYPE_BOOL(true)', true,
         `n', `true', `Enable locking by session ID',
         `If specified, ', `If true ',
         `M4_LINES(`locking by session ID is enabled.')')m4_dnl
m4_dnl
M4_PARAM(`', `login', `name-',
         `M4_TYPE_BOOL()', false,
         `l', `', `Make the shell a login shell',
         `If specified, ', `If true ',
         `M4_LINES(`the shell is signalled to act as a login shell.',
                   `This is done by prepending argv[0] of the shell',
                   `with a dash character.')')m4_dnl
m4_dnl
M4_PARAM(`', `interactive', `name-',
         `M4_TYPE_BOOL()', false,
         `i', `', `Make the shell an interactive shell',
         `If specified, ', `If true ',
         `M4_LINES(`tlog-rec-session passes the -i option to the shell.')')m4_dnl
m4_dnl
M4_PARAM(`', `command', `opts',
         `M4_TYPE_BOOL()', false,
         `c', `', `Execute shell commands',
         `If specified, ', `If true ',
         `M4_LINES(`tlog-rec-session passes the -c option to the shell,',
                   `followed by all the positional arguments, which specify the shell',
                   `commands to execute along with command name and its arguments.')')m4_dnl
m4_dnl
M4_PARAM(`', `notice', `file-env',
         `M4_TYPE_STRING(`\nATTENTION! Your session is being recorded!\n\n')',
         true,
         `', `=TEXT', `Print TEXT message before starting recording',
         `TEXT is a ', `A ',
         `M4_LINES(`message which will be printed before starting',
                   `recording and the user shell. Can be used to warn',
                   `the user that the session is recorded.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl Include common schema, but limit its scope to environment
m4_dnl
m4_pushdef(`_M4_PARAM', `M4_PARAM(`$1', `$2', `$3env', m4_shiftn(3, $@))')m4_dnl
m4_include(`rec_common_conf_schema.m4')m4_dnl
m4_popdef(`_M4_PARAM')m4_dnl
m4_dnl
m4_ifelse(M4_JOURNAL_ENABLED(), `1',
`M4_PARAM(`', `writer', `file-env',
          `M4_TYPE_CHOICE(`journal', `journal', `syslog', `file')', true,
          `w', `=STRING', `Use STRING log writer (journal/syslog/file, default journal)',
          `STRING is the ', `The ',
          `M4_LINES(`type of "log writer" to use for logging. The writer needs',
                    `to be configured using its dedicated parameters.')')',
`M4_PARAM(`', `writer', `file-env',
          `M4_TYPE_CHOICE(`syslog', `syslog', `file')', true,
          `w', `=STRING', `Use STRING log writer (syslog/file, default syslog)',
          `STRING is the ', `The ',
          `M4_LINES(`type of "log writer" to use for logging. The writer needs',
                    `to be configured using its dedicated parameters.')')')m4_dnl
m4_dnl
