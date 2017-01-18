m4_dnl
m4_dnl Tlog-rec configuration schema
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
m4_dnl M4_LINES - specify text as a list of lines without terminating newlines
m4_dnl Arguments:
m4_dnl
m4_dnl      $@ Text lines
m4_dnl
m4_dnl
m4_dnl M4_CONTAINER - describe a container
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 Container prefix (`' for root)
m4_dnl      $2 Container name
m4_dnl      $3 Container description
m4_dnl
m4_dnl
m4_dnl M4_PARAM - describe a parameter
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 Container prefix (`' for root)
m4_dnl      $2 Parameter name
m4_dnl      $3 Parameter origin, one of "file", "env", "name", "opts", or "args"
m4_dnl      $4 Type, must be an invocation of M4_TYPE_*.
m4_dnl      $5 `true' if has default value, `false' otherwise
m4_dnl      $6 Option letter
m4_dnl      $7 Option value placeholder
m4_dnl      $8 Option title
m4_dnl      $9 Description, must be an invocation of M4_LINES
m4_dnl
m4_dnl M4_TYPE_INT - describe integer type
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 Default value
m4_dnl      $2 Minimum value
m4_dnl
m4_dnl M4_TYPE_STRING - describe string type
m4_dnl
m4_dnl      $1 Default value
m4_dnl
m4_dnl M4_TYPE_BOOL - describe boolean type
m4_dnl
m4_dnl      $1 Default value
m4_dnl
m4_dnl M4_TYPE_CHOICE - describe a string choice type
m4_dnl Arguments:
m4_dnl
m4_dnl      $1 Default value
m4_dnl      $@ Choices
m4_dnl
m4_dnl M4_TYPE_STRING_ARRAY - describe a string array type
m4_dnl Arguments:
m4_dnl
m4_dnl      $@ Default values
m4_dnl
M4_PARAM(`', `args', `args',
         `M4_TYPE_STRING_ARRAY()', false,
         `', `', `',
         `M4_LINES(`Non-option positional command-line arguments.')')m4_dnl
m4_dnl
M4_PARAM(`', `help', `opts',
         `M4_TYPE_BOOL(false)', true,
         `h', `', `Output a command-line usage message and exit',
         `M4_LINES(`')')m4_dnl
m4_dnl
M4_PARAM(`', `version', `opts',
         `M4_TYPE_BOOL(false)', true,
         `v', `', `Output version information and exit',
         `M4_LINES(`')')m4_dnl
m4_dnl
M4_PARAM(`', `shell', `file',
         `M4_TYPE_STRING(`/bin/bash')', true,
         `s', `=SHELL', `Spawn the specified SHELL',
         `M4_LINES(`The path to the shell executable that should be spawned.')')m4_dnl
m4_dnl
M4_PARAM(`', `login', `name',
         `M4_TYPE_BOOL()', false,
         `l', `', `Make the shell a login shell',
         `M4_LINES(`If set to true, the shell is signalled to act as a login shell.',
                   `This is done by prepending argv[0] of the shell',
                   `with a dash character.')')m4_dnl
m4_dnl
M4_PARAM(`', `command', `opts',
         `M4_TYPE_BOOL()', false,
         `c', `', `Execute shell commands',
         `M4_LINES(`If set to true, tlog-rec passes the -c option to the shell,',
                   `followed by all the positional arguments, which specify the shell',
                   `commands to execute along with command name and its arguments.')')m4_dnl
m4_dnl
M4_PARAM(`', `notice', `file',
         `M4_TYPE_STRING(`\nATTENTION! Your session is being recorded!\n\n')',
         true,
         `', `=TEXT', `Print TEXT message before starting recording',
         `M4_LINES(`A message which will be printed before starting',
                   `recording and the user shell. Can be used to warn',
                   `the user that the session is recorded.')')m4_dnl
m4_dnl
M4_PARAM(`', `latency', `file',
         `M4_TYPE_INT(10, 1)', true,
         `', `=SECONDS', `Cache captured data SECONDS seconds before logging',
         `M4_LINES(`The data which does not exceed maximum payload',
                   `stays in memory and is not logged until this number of',
                   `seconds elapses.')')m4_dnl
m4_dnl
M4_PARAM(`', `payload', `file',
         `M4_TYPE_INT(2048, 32)', true,
         `', `=BYTES', `Limit encoded data to BYTES bytes',
         `M4_LINES(`Maximum encoded data (payload) size per message, bytes.',
                   `As soon as payload exceeds this number of bytes,',
                   `it is formatted into a message and logged.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/log', `Logged data set')m4_dnl
m4_dnl
M4_PARAM(`/log', `input', `file',
         `M4_TYPE_BOOL(false)', true,
         `', `[=BOOL]', `Enable/disable logging user input',
         `M4_LINES(`If specified as true, user input is logged.')')m4_dnl
m4_dnl
M4_PARAM(`/log', `output', `file',
         `M4_TYPE_BOOL(true)', true,
         `', `[=BOOL]', `Enable/disable logging program output',
         `M4_LINES(`If specified as true, terminal output is logged.')')m4_dnl
m4_dnl
M4_PARAM(`/log', `window', `file',
         `M4_TYPE_BOOL(true)', true,
         `', `[=BOOL]', `Enable/disable logging terminal window size changes',
         `M4_LINES(`If specified as true, terminal window size changes are logged.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_PARAM(`', `writer', `file',
         `M4_TYPE_CHOICE(`syslog', `syslog', `file')', true,
         `w', `=STRING', `Use STRING log writer (syslog/file, default syslog)',
         `M4_LINES(`The type of "log writer" to use for logging. The writer needs',
                   `to be configured using its dedicated parameters.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/file', `File writer')m4_dnl
m4_dnl
M4_PARAM(`/file', `path', `file',
         `M4_TYPE_STRING()', false,
         `', `=FILE', `Log to FILE file',
         `M4_LINES(`The "file" writer log file path.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/syslog', `Syslog writer')m4_dnl
m4_dnl
M4_PARAM(`/syslog', `facility', `file',
         `M4_TYPE_CHOICE(`authpriv',
                         `auth',
                         `authpriv',
                         `cron',
                         `daemon',
                         `ftp',
                         `kern',
                         `local0',
                         `local1',
                         `local2',
                         `local3',
                         `local4',
                         `local5',
                         `local6',
                         `local7',
                         `lpr',
                         `mail',
                         `news',
                         `syslog',
                         `user',
                         `uucp')',
         true,
         `', `=STRING', `Log with STRING syslog facility',
         `M4_LINES(`Syslog facility the "syslog" writer should use for the messages.')')m4_dnl
m4_dnl
M4_PARAM(`/syslog', `priority', `file',
         `M4_TYPE_CHOICE(`info',
                         `emerg',
                         `alert',
                         `crit',
                         `err',
                         `warning',
                         `notice',
                         `info',
                         `debug')',
         true,
         `', `=STRING', `Log with STRING syslog priority',
         `M4_LINES(`Syslog priority the "syslog" writer should use for the messages.')')m4_dnl
m4_dnl
