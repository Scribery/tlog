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
M4_PARAM(`', `follow', `opts-',
         `M4_TYPE_BOOL(false)', true,
         `f', `', `Wait for and play back new messages',
         `M4_LINES(`If true, then when the end of the recorded session is reached, wait',
                   `for new messages to be added and play them back when they appear.')')m4_dnl
m4_dnl
M4_PARAM(`', `forward', `opts-',
         `M4_TYPE_BOOL(false)', true,
         `F', `', `Play recording to the end at once',
         `M4_LINES(`If true, then play recording ignoring timing, as fast as possible,',
                   `until the end, then resume respecting timing. Useful when following.')')m4_dnl
m4_dnl
M4_PARAM(`', `paused', `opts-',
         `M4_TYPE_BOOL(false)', true,
         `p', `', `Start playback paused',
         `M4_LINES(`If true, then playback is started in a paused state,',
                   `but after forwarding, if enabled.')')m4_dnl
m4_dnl
M4_PARAM(`', `reader', `file-',
         `M4_TYPE_CHOICE(`file', `file', `journal', `es')', true,
         `r', `=STRING', `Use STRING log reader (file/journal/es, default file)',
         `M4_LINES(`The type of "log reader" to use for retrieving log messages. The chosen',
                   `reader needs to be configured using its own dedicated parameters.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/file', `File reader')m4_dnl
m4_dnl
M4_PARAM(`/file', `path', `file-',
         `M4_TYPE_STRING()', false,
         `i', `=FILE', `Read log from FILE file',
         `M4_LINES(`The "file" reader log file path.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/es', `ElasticSearch reader')m4_dnl
m4_dnl
M4_PARAM(`/es', `baseurl', `file-',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `ElasticSearch URL without query or fragment parts',
         `M4_LINES(`The base URL to request ElasticSearch through. Should not',
                   `contain the query (?...) or fragment (#...) parts.')')m4_dnl
m4_dnl
M4_PARAM(`/es', `query', `file-',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `ElasticSearch query',
         `M4_LINES(`The query string to send to ElasticSearch')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/journal', `Systemd journal reader')m4_dnl
m4_dnl
M4_PARAM(`/journal', `since', `opts-',
         `M4_TYPE_INT(0, 0)', true,
         `S', `=SECONDS', `Start searching journal at SECONDS since epoch',
         `M4_LINES(`Number of seconds since epoch to seek to before searching',
                   `for the first matching log entry.')')m4_dnl
m4_dnl
M4_PARAM(`/journal', `until', `opts-',
         `M4_TYPE_INT(0x7fffffffffffffff, 0)', true,
         `U', `=SECONDS', `Stop searching journal at SECONDS since epoch',
         `M4_LINES(`Number of seconds since epoch at which searching for',
                   `log entries should stop.')')m4_dnl
m4_dnl
M4_PARAM(`/journal', `match', `opts-',
         `M4_TYPE_STRING_ARRAY()', true,
         `M', `=STRING', `Add STRING to journal match symbol list',
         `M4_LINES(`Each entry specifies a journal match symbol: either a name-value',
                   `pair, according to sd_journal_add_match(3), or an "OR" or "AND"',
                   `string signifying disjunction or conjunction, as with',
                   `sd_journal_add_disjunction and sd_journal_add_conjunction(3)')')m4_dnl
m4_dnl
M4_PARAM(`', `persist', `file-',
         `M4_TYPE_BOOL(false)', true,
         `', `', `Ignore quit key and signals from keyboard',
         `M4_LINES(`If true, ignore any keyboard-generated signals and the quit key.')')m4_dnl
m4_dnl
