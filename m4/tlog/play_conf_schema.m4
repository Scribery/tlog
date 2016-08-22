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
M4_PARAM(`', `follow', `opts',
         `M4_TYPE_BOOL(false)', true,
         `f', `', `Wait for and play back new messages',
         `M4_LINES(`If true, then when the end of the recorded session is reached, wait',
                   `for new messages to be added and play them back when they appear.')')m4_dnl
m4_dnl
M4_PARAM(`', `reader', `file',
         `M4_TYPE_CHOICE(`file', `file', `es')', true,
         `r', `=STRING', `Use STRING log reader (file/es, default file)',
         `M4_LINES(`The type of "log reader" to use for retrieving log messages. The chosen',
                   `reader needs to be configured using its own dedicated parameters.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/file', `File reader')m4_dnl
m4_dnl
M4_PARAM(`/file', `path', `file',
         `M4_TYPE_STRING()', false,
         `', `=FILE', `Read log from FILE file',
         `M4_LINES(`The "file" reader log file path.')')m4_dnl
m4_dnl
m4_dnl
m4_dnl
M4_CONTAINER(`', `/es', `ElasticSearch reader')m4_dnl
m4_dnl
M4_PARAM(`/es', `baseurl', `file',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `ElasticSearch URL without query or fragment parts',
         `M4_LINES(`The base URL to request ElasticSearch through. Should not',
                   `contain the query (?...) or fragment (#...) parts.')')m4_dnl
m4_dnl
M4_PARAM(`/es', `query', `file',
         `M4_TYPE_STRING()', false,
         `', `=STRING', `ElasticSearch query',
         `M4_LINES(`The query string to send to ElasticSearch')')m4_dnl
m4_dnl
