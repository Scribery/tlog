m4_dnl
m4_dnl Tlog common recording configuration schema
m4_dnl
m4_dnl See conf_schema.m4 for macro invocations expected here.
m4_dnl
m4_dnl This file invokes _M4_PARAM macro so it can be overriden by including
m4_dnl files before it becomes M4_PARAM.
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
_M4_PARAM(`', `latency', `file-',
          `M4_TYPE_INT(10, 1)', true,
          `', `=SECONDS', `Cache captured data SECONDS seconds before logging',
          `M4_LINES(`The encoded data which does not reach payload size',
                    `stays in memory and is not logged until this number of',
                    `seconds elapses.')')m4_dnl
m4_dnl
_M4_PARAM(`', `payload', `file-',
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
_M4_PARAM(`/log', `input', `file-',
          `M4_TYPE_BOOL(false)', true,
          `', `[=BOOL]', `Enable/disable logging user input',
          `M4_LINES(`If specified as true, user input is logged.')')m4_dnl
m4_dnl
_M4_PARAM(`/log', `output', `file-',
          `M4_TYPE_BOOL(true)', true,
          `', `[=BOOL]', `Enable/disable logging program output',
          `M4_LINES(`If specified as true, terminal output is logged.')')m4_dnl
m4_dnl
_M4_PARAM(`/log', `window', `file-',
          `M4_TYPE_BOOL(true)', true,
          `', `[=BOOL]', `Enable/disable logging terminal window size changes',
          `M4_LINES(`If specified as true, terminal window size changes are logged.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/limit', `Logging limit')m4_dnl
m4_dnl
_M4_PARAM(`/limit', `rate', `file-',
          `M4_TYPE_INT(16384, 0)', true,
          `', `=NUMBER', `Set logging rate limit to NUMBER of message bytes/sec',
          `M4_LINES(`Maximum rate messages could be logged at, bytes/sec.')')m4_dnl
m4_dnl
_M4_PARAM(`/limit', `burst', `file-',
          `M4_TYPE_INT(32768, 0)', true,
          `', `=NUMBER', `Set logging burst limit to NUMBER of message bytes',
          `M4_LINES(`Number of bytes by which logged messages are allowed to exceed',
                    `the rate limit momentarily, i.e. "burstiness", bytes.')')m4_dnl
m4_dnl
_M4_PARAM(`/limit', `action', `file-',
          `M4_TYPE_CHOICE(`pass', `pass', `delay', `drop')', true,
          `', `=STRING', `Perform STRING action above limits (pass/delay/drop)',
          `M4_LINES(`If set to "pass" no logging limits will be applied.',
                    `If set to "delay", logging will be throttled.',
                    `If set to "drop", messages exceeding limits will be dropped.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/file', `File writer')m4_dnl
m4_dnl
_M4_PARAM(`/file', `path', `file-',
          `M4_TYPE_STRING()', false,
          `o', `=FILE', `Log to FILE file',
          `M4_LINES(`The "file" writer log file path.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/syslog', `Syslog writer')m4_dnl
m4_dnl
_M4_PARAM(`/syslog', `facility', `file-',
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
_M4_PARAM(`/syslog', `priority', `file-',
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
m4_dnl
m4_dnl
M4_CONTAINER(`', `/journal', `Journal writer')m4_dnl
m4_dnl
_M4_PARAM(`/journal', `priority', `file-',
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
          `', `=STRING', `Log with STRING syslog-style priority',
          `M4_LINES(`Syslog-style priority the "journal" writer should use for the messages.')')m4_dnl
m4_dnl
