Tlog
====

[![Build Status](https://travis-ci.org/Scribery/tlog.svg?branch=master)](https://travis-ci.org/Scribery/tlog)
[![Coverage Status](https://coveralls.io/repos/github/Scribery/tlog/badge.svg?branch=master)](https://coveralls.io/github/Scribery/tlog?branch=master)

`Tlog` is a terminal I/O recording and playback package suitable for
implementing [centralized user session recording][session_recording].
At the moment it is not ready for production and is to be considered
development preview quality.

Whereas most other similar packages write the recorded data to a file in their
own format, or upload it to a custom server, `tlog` sends it to a logging
service. The standard syslog interface is supported already, with journald
possibly to come. The recorded data is [encoded][log_format] in JSON in
a way which keeps it human-readable and searchable as much as possible.

The primary purpose of logging in JSON format is to eventually deliver the
recorded data to a storage service such as [Elasticsearch][elasticsearch],
where it can be searched and queried, and from where it can be played back.

`Tlog` is naturally split into two tools: `tlog-rec` and `tlog-play` - for
recording and playback respectively. `Tlog-rec` is intended to be the user's
login shell. It puts itself between the actual user's shell and the terminal
upon user login, logging everything that passes through. At the moment,
`tlog-play` can playback recordings from Elasticsearch or from a file written
by `tlog-rec` with `file` writer selected.

Building
--------

Build dependencies are cURL and json-c, which development packages are
`json-c-devel` and `libcurl-devel` on RPM-based distros, and `libjson-c-dev`
and any of `libcurl-*-dev` on Debian-based distros.

If you'd like to build `tlog` from the Git source tree, you need to first
generate the build system files:

    autoreconf -i -f

After that, or if you're building a [release source tarball][releases], you
need to follow the usual configure & make approach:

    ./configure --prefix=/usr --sysconfdir=/etc && make

From the same source tarball you can build an SRPM package:

    rpmbuild -ts <tarball>

Or an RPM package:

    rpmbuild -tb <tarball>

Installing
----------

You can use one of the [release binary RPM packages][releases] and install
them with your favorite tool. The RPM package does all the necessary setup for
you.

Otherwise, if you built `tlog` from source, you can install it with the usual
`make install`:

    sudo make install

If you are recording other users, and don't want them to be able to affect the
recording process, make sure you make the `/usr/bin/tlog-rec` executable
SUID/SGID to separate and dedicated user and group. It doesn't require running
as root and will be safer with a regular, dedicated user.

You will also need to create the session lock directory `/var/run/tlog` and
make it writable (only) for the user(s) `tlog-rec` runs as. On systems where
(/var)/run is a tmpfs you will also need to make sure the session lock
directory is recreated on the next boot. In that case, on systems with systemd
you'll need to create a tmpfiles.d configuration file, and on init.d systems -
a startup script.

Testing
-------

You can test if session recording and playback work in general with a freshly
installed tlog, by recording a session into a file and then playing it back.

Usage
-----

### Recording to a file

To record into a file execute `tlog-rec` on the command line as such:

    tlog-rec --writer=file --file-path=tlog.log

### Playing back from a file

Both during and after the recording you can play the session back with
`tlog-play`:

    tlog-play --reader=file --file-path=tlog.log

### Recording a user

Change the shell of the user to be recorded to `tlog-rec`:

    sudo usermod -s /usr/bin/tlog-rec <user>

Login as the user on a text terminal. By default the recorded terminal data
will be delivered to syslog with facility "authpriv" and priority "info", and
will appear in journal, if you use journald, in `/var/log/auth.log` on
Debian-based systems, or in `/var/log/secure` on Fedora and derived systems.

Customize `tlog-rec` configuration in `/etc/tlog/tlog-rec.conf` as necessary
(see `tlog-rec.conf(5)` for details).

### Recording to Elasticsearch

Rsyslog can be set up to deliver tlog messages to Elasticsearch. First of
all increase the maximum message size to be 1k more than the `tlog-rec` payload.
The default payload is 2kB, so the `rsyslog` maximum message size needs to be
"3k" if the defaults are used:

    $MaxMessageSize 3k

The line above needs to be above any network setup in `rsyslog.conf` (put it
at the top to be safe).

Then the Elasticsearch output module needs to be loaded:

    $ModLoad omelasticsearch

#### Massaging JSON

Before sending tlog messages to Elasticsearch they need to be reformatted
and real time timestamp needs to be added, which can be done with this
`rsyslog` template:

    template(name="tlog" type="list") {
        constant(value="{")
        property(name="timegenerated"
                 outname="timestamp"
                 format="jsonf"
                 dateFormat="rfc3339")
        constant(value=",")
        property(name="msg"
                 regex.expression="{\\(.*\\)"
                 regex.submatch="1")
        constant(value="\n")
    }

#### Filtering out tlog messages

Then, a rule routing messages originating from tlog to Elasticsearch
needs to be added. If you installed v3 or later tlog RPM package, or set up
`tlog-rec` as SUID/SGID to a dedicated user yourself, then the rule can use
that user ID to filter genuine tlog messages securely.

If your rsyslog receives messages from journald, with the `imjournal` module,
then the rule condition should be:

    if $!_UID == "<TLOG_UID>" then {
        # ... actions ...
    }

Note that the above would only work with rsyslog v8.17.0 and later, due to an
issue preventing it from parsing variable names starting with underscore.

If your rsyslog receives messages via syslog(1) socket by itself, with the
imuxsock module, you need to enable the module's `Annotate` and `ParseTrusted`
options. E.g. like this:

    module(load="imuxsock" SysSock.Annotate="on" SysSock.ParseTrusted="on")

And then the rule condition should be:

    if $!uid == "<TLOG_UID>" then {
        # ... actions ...
    }

The `<TLOG_UID>` above should be replaced with the UID your `tlog-rec` runs
as.

Otherwise you'll need to filter by something else. For example the program
name (`ident` argument to syslog(3)), which tlog specifies as `tlog`. In that
case the condition could be:

    if $programname == "tlog" then {
        # ... actions ...
    }

However, note that any program is able to log with that program name and thus
spoof tlog messages.

#### Sending the messages

Once your rule condition is established, you can add the actual action sending
the messages to Elasticsearch:

    action(name="tlog-elasticsearch"
           type="omelasticsearch"
           server="localhost"
           searchIndex="tlog-rsyslog"
           searchType="tlog"
           bulkmode="on"
           template="tlog")

The action above would send messages formatted with the `tlog` template,
described above, to an Elasticsearch server running on localhost and default
port, and would put them into index `tlog-rsyslog` with type `tlog`, using the
bulk interface.

Add the following action if you want to also send tlog messages to a dedicated
file for debugging:

    action(name="tlog-file"
           type="omfile"
           file="/var/log/tlog.log"
           fileCreateMode="0600"
           template="tlog")

Further, if you don't want tlog messages delivered anywhere else you can add
the discard action (`~`) after both of those:

    ~

If you'd like to exclude tlog messages from *any* other logs remember to put
its rule before any other rules in `rsyslog.conf`.

Here is a complete example of a rule matching messages arriving from `tlog-rec`
running as user with UID 123, delivered from journald. It sends them to
Elasticsearch running on localhost with default port, puts them into
`tlog-rsyslog` index with type `tlog`, using bulk interface, stores them in
`/var/log/tlog.log` file, and then stops processing, not letting them get
anywhere else.

	if $!_UID == "123" then {
		action(name="tlog-elasticsearch"
			   type="omelasticsearch"
			   server="localhost"
			   searchIndex="tlog-rsyslog"
			   searchType="tlog"
			   bulkmode="on"
			   template="tlog")
		action(name="tlog-file"
			   type="omfile"
			   file="/var/log/tlog.log"
			   fileCreateMode="0600"
			   template="tlog")
		~
	}

### Playing back from Elasticsearch

Once you got tlog messages to Elasticsearch, you can play them back using the
still rudimentary `tlog-play` command-line tool. You will need to tell it to
use the Elasticsearch reader (`es`), supply it with the Elasticsearch base
URL, and the query string, which would match the messages for your session.

The base URL should point to the `_search` endpoint for your type and index.
E.g. a base URL for index `tlog-rsyslog` and type `tlog` on localhost would
be:

    http://localhost:9200/tlog-rsyslog/tlog/_search

The query string should follow the [Elasticsearch query string
syntax][query_string_syntax]. E.g. to look for session #17 which happened in
the last week on host `server`, you can use this query string:

    host:server AND timestamp:>=now-7d AND session:17

Use `--reader` (or just `-r`), `--es-baseurl` and `--es-query` options to
specify the reader, base URL, and the query string respectively. The full
command for the above parameters could look like this:

    tlog-play -r es \
              --es-baseurl=http://localhost:9200/tlog-rsyslog/tlog/_search \
              --es-query='host:server AND timestamp:>=now-7d AND session:17'

If you're playing back an ongoing session, adding the `--follow` or `-f`
option will make `tlog-play` wait for more messages after it plays back all
that were logged so far. Just like `tail -f` will wait for more lines to be
added to a file it's outputting.

Interrupt `tlog-play` (e.g. press Ctrl-C) to stop the playback at any moment.

Instead of specifying the reader and the base URL on the command line every
time, you can put them in `/etc/tlog/tlog-play.conf` configuration file.

#### Limitations

Currently `tlog-play` is very limited. It expects to receive all messages
belonging to a single session, uninterrupted, beginning from the first one. It
doesn't provide a way to speed up, rewind, or pause playback. It polls
Elasticsearch for new messages, instead of asking to be updated when they
appear. Even though the messages contain recorded terminal window (re)sizes,
it cannot resize its own terminal to fit the output better.

Further development will be addressing these and other issues either in
`tlog-play`, and/or in the upcoming Web UI, which will be a separate project.

[session_recording]: http://scribery.github.io/
[log_format]: doc/log_format.md
[elasticsearch]: https://www.elastic.co/products/elasticsearch
[releases]: https://github.com/Scribery/tlog/releases
[query_string_syntax]: https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-query-string-query.html#query-string-syntax
