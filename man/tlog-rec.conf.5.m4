m4_include(`man.m4')m4_dnl
.\" Process this file with
.\" groff -man -Tascii tlog-rec.conf.5
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
.TH tlog-rec.conf "5" "February 2016" "Tlog"
.SH NAME
tlog-rec.conf \- tlog-rec configuration file

.SH DESCRIPTION
.B tlog-rec.conf
is a JSON-format configuration file for
.B tlog-rec
program.
Contrary to the strict JSON specification, both C and C++ style comments are
allowed in the file.

The file must contain a single JSON object with the objects and fields
described below.
Almost all of them are optional and assume a default value.
However, those that do require a value can still be omitted and specified to
.B tlog-rec
in other ways: through environment variables or command line.

.SH OBJECTS AND FIELDS
M4_MAN_CONF()
.SH EXAMPLES
.TP
A config specifying only a shell:
.nf

{
    "shell": "/usr/bin/zsh"
}
.fi

.TP
A config enabling logging user input:
.nf

{
    "log": {
        "input": true
    }
}
.fi

.TP
A config specifying logging to a file:
.nf

{
    "writer": "file"
    "file" : {
        "path": "/var/log/tlog-rec.log"
    }
}
.fi

.SH SEE ALSO
tlog-rec(8), http://json.org/

.SH AUTHOR
Nikolai Kondrashov <spbnick@gmail.com>
