m4_dnl
m4_dnl Tlog-play configuration schema
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
m4_include(`conf_schema.m4')m4_dnl
m4_dnl
M4_PARAM(`', `speed', `opts-',
         `M4_TYPE_DOUBLE(1, 0)', true,
         `s', `=NUMBER', `Set playback speed multiplier to NUMBER',
         `NUMBER is a ', `A ',
         `M4_LINES(`floating-point number to multiply playback speed by.',
                   `Can be adjusted during playback.')')m4_dnl
m4_dnl
M4_PARAM(`', `follow', `opts-',
         `M4_TYPE_BOOL(false)', true,
         `f', `', `Wait for and play back new messages',
         `If specified, ', `If true, ',
         `M4_LINES(`when the end of the recorded session is reached, wait',
                   `for new messages to be added and play them back when they appear.')')m4_dnl
m4_dnl
M4_PARAM(`', `goto', `opts-',
         `M4_TYPE_STRING()', false,
         `g', `=STRING', `Fast-forward to STRING time (start/end/HH:MM:SS.sss)',
         `STRING is a ', `A ',
         `M4_LINES(`logical location, or a time to which recording should be fast-forwarded.',
                   `Can be a "start", or an "end" string, or a timestamp formatted as',
                   `HH:MM:SS.sss, where any part can be omitted to mean zero.')')m4_dnl
m4_dnl
M4_PARAM(`', `to', `opts-',
         `M4_TYPE_STRING()', false,
         `t', `=STRING', `Stop to STRING time (start/end/HH:MM:SS.sss)',
         `STRING is a ', `A ',
         `M4_LINES(`logical location, or a time to which recording should be stopped at specific time.',
         `Can be a "start", or an "end" string, or a timestamp formatted as',
         `HH:MM:SS.sss, where any part can be omitted to mean zero.')')m4_dnl
m4_dnl
M4_PARAM(`', `paused', `opts-',
         `M4_TYPE_BOOL(false)', true,
         `p', `', `Start playback paused',
         `If specified, ', `If true, ',
         `M4_LINES(`playback is started in a paused state.')')m4_dnl
m4_dnl
m4_ifelse(M4_JOURNAL_ENABLED(), `1',
`M4_PARAM(`', `reader', `file-',
          `M4_TYPE_CHOICE(`file', `file', `journal', `es')', true,
          `r', `=STRING', `Use STRING log reader (file/journal/es, default file)',
          `STRING is the ', `The ',
          `M4_LINES(`type of "log reader" to use for retrieving log messages. The chosen',
                    `reader needs to be configured using its own dedicated parameters.')')',
`M4_PARAM(`', `reader', `file-',
          `M4_TYPE_CHOICE(`file', `file', `es')', true,
          `r', `=STRING', `Use STRING log reader (file/es, default file)',
          `STRING is the ', `The ',
          `M4_LINES(`type of "log reader" to use for retrieving log messages. The chosen',
                    `reader needs to be configured using its own dedicated parameters.')')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/file', `File reader')m4_dnl
m4_dnl
M4_PARAM(`/file', `path', `file-',
         `M4_TYPE_STRING()', false,
         `i', `=FILE', `Read log from FILE file',
         `FILE is the ', `The ',
         `M4_LINES(`path to the file the "file" reader should read logs from.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/es', `Elasticsearch reader')m4_dnl
m4_dnl
M4_PARAM(`/es', `baseurl', `file-',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `Elasticsearch URL without query or fragment parts',
         `STRING is the ', `The ',
         `M4_LINES(`base URL to request Elasticsearch through. Should not',
                   `contain query (?...) or fragment (#...) parts.')')m4_dnl
m4_dnl
M4_PARAM(`/es', `query', `file-',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `Elasticsearch query',
         `STRING is the ', `The ',
         `M4_LINES(`query string to send to Elasticsearch')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
m4_ifelse(M4_JOURNAL_ENABLED(), `1', `m4_dnl
m4_dnl
M4_CONTAINER(`', `/journal', `Systemd journal reader')m4_dnl
m4_dnl
M4_PARAM(`/journal', `since', `opts-',
         `M4_TYPE_INT(0, 0)', true,
         `S', `=SECONDS', `Start searching journal at SECONDS since epoch',
         `SECONDS is the ', `The ',
         `M4_LINES(`number of seconds since epoch to seek to before searching',
                   `for the first matching log entry.')')m4_dnl
m4_dnl
M4_PARAM(`/journal', `until', `opts-',
         `M4_TYPE_INT(0x7fffffffffffffff, 0)', true,
         `U', `=SECONDS', `Stop searching journal at SECONDS since epoch',
         `SECONDS is the ', `The ',
         `M4_LINES(`number of seconds since epoch at which searching for',
                   `log entries should stop.')')m4_dnl
m4_dnl
M4_PARAM(`/journal', `match', `opts-',
         `M4_TYPE_STRING_ARRAY()', true,
         `M', `=STRING', `Add STRING to journal match symbol list',
         `Each STRING ', `An array of journal match symbols. Each entry ',
         `M4_LINES(`specifies a journal match symbol: either a name-value',
                   `pair, according to sd_journal_add_match(3), or an "OR" or "AND"',
                   `string signifying disjunction or conjunction, as with',
                   `sd_journal_add_disjunction(3) and sd_journal_add_conjunction(3)')')m4_dnl
m4_dnl
')m4_dnl m4_ifelse M4_JOURNAL_ENABLED
m4_dnl
M4_PARAM(`', `persist', `file-',
         `M4_TYPE_BOOL(false)', true,
         `', `', `Ignore quit key and signals from keyboard',
         `If specified, ', `If true, ',
         `M4_LINES(`ignore any keyboard-generated signals and the quit key.')')m4_dnl
m4_dnl
M4_PARAM(`', `lax', `file-',
         `M4_TYPE_BOOL(false)', true,
         `', `', `Ignore missing (dropped) log messages',
         `If specified, ', `If true, ',
         `M4_LINES(`ignore missing (dropped, or lost) log messages.',
                   `Otherwise report an error and abort when a message is missing.')')m4_dnl
m4_dnl
