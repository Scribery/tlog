Tlog
====

[![Build Status](https://travis-ci.org/Scribery/tlog.svg?branch=master)](https://travis-ci.org/Scribery/tlog)
[![Coverage Status](https://coveralls.io/repos/github/Scribery/tlog/badge.svg?branch=master)](https://coveralls.io/github/Scribery/tlog?branch=master)

`Tlog` is a terminal I/O recording and playback package suitable for
implementing [centralized user session recording][session_recording].

Whereas most other similar packages write the recorded data to a file in their
own format, or upload it to a custom server, `tlog` sends it to a logging
service. Both the standard syslog and the journald interfaces are supported.
The recorded data is [encoded][log_format] in JSON in a way which keeps it
human-readable and searchable as much as possible.

The primary purpose of logging in JSON format is to eventually deliver the
recorded data to a storage service such as [Elasticsearch][elasticsearch],
where it can be searched and queried, and from where it can be played back.

`Tlog` contains three tools: `tlog-rec` for recording terminal I/O of programs
or shells in general, `tlog-rec-session` for recording I/O of whole terminal
sessions, with protection from recorded users, and `tlog-play` for playing
back the recordings. You can run `tlog-rec` for testing or recording specific
commands or shell sessions for yourself, or you can integrate it into another
solution. `Tlog-rec-session` is intended to be a user's login shell. It puts
itself between the actual user's shell and the terminal upon user login,
logging everything that passes through. Lastly, `tlog-play` can playback
recordings from Elasticsearch or from a file, made with either `tlog-rec` or
`tlog-rec-session`. There is no difference in log format between `tlog-rec`
and `tlog-rec-session`.

Building
--------

Build dependencies are systemd, cURL, json-c, and libutempter, which development
packages are `systemd-devel`, `json-c-devel`, `libcurl-devel`, and
`libutempter-devel` on RPM-based distros, and `pkg-config`, `libjson-c-dev`,
`libsystemd-journal-dev`/`libsystemd-dev`, `libcurl-*-dev`, and `libutempter-dev`
on Debian-based distros.

To build from Git you'll need `autoconf`, `automake` and `libtool` packages.
For creating RPM package `rpm-build` is also required.

If Systemd Journal support is not required, it can be disabled with
configure's `--disable-journal` option, removing the systemd dependency as
well.

Updating the system utmp and wtmp files can be enabled with the
`--enable-utempter` configure option, utilizing the libutempter library.

If you'd like to build `tlog` from the Git source tree, you need to first
generate the build system files:

    autoreconf -i -f

After that, or if you're building a [release source tarball][releases], you
need to follow the usual configure & make approach:

    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var && make

To generate a source tarball:

    ./configure --prefix=/usr --sysconfdir=/etc && make dist

From a source tarball you can build an SRPM package:

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

If you are recording other user sessions, and don't want them to be able to
affect the recording process, make sure you use `tlog-rec-session` and its
executable is SUID/SGID to separate and dedicated user and group. It doesn't
require running as root and will be safer with a regular, dedicated user.

You will also need to create the session lock directory `/var/run/tlog` and
make it writable (only) for the user(s) `tlog-rec-session` runs as. On systems
where (/var)/run is a tmpfs you will also need to make sure the session lock
directory is recreated on the next boot. In that case, on systems with systemd
you'll need to create a tmpfiles.d configuration file, and on init.d systems -
a startup script, creating the directory for you.

Testing
-------

You can test if session recording and playback work in general with a freshly
installed tlog, by recording a session into a file with `tlog-rec` and then
playing it back with `tlog-play`.

Usage
-----

### Recording to a file

To record into a file, execute `tlog-rec` on the command line as such:

    tlog-rec --writer=file --file-path=tlog.log

### Playing back from a file

Both during, and after the recording you can play the session back with
`tlog-play`:

    tlog-play --reader=file --file-path=tlog.log

### Recording to Systemd Journal

To record into the Systemd Journal, execute `tlog-rec` as such:

    tlog-rec --writer=journal

Along with the regular JSON log messages, when recording to Journal, tlog
copies a few JSON fields to Journal fields (unless explicitly disabled) to aid
searching and extracting (parts of) particular recordings:

* `TLOG_USER` - the user the recording was started as (`user` in JSON),
* `TLOG_SESSION` - the audit session ID of the recording process
   (`session` in JSON),
* `TLOG_REC` - host-unique recording ID (`rec` in JSON),
* `TLOG_ID` - log message ID within the recording (`id` in JSON).

### Playing back from Systemd Journal

In general, selecting Journal log entries for playback is done using Journal
matches and timestamp limits, with `-M/--journal-match`, `-S/--journal-since`,
and `-U/--journal-until` options.

In practice however, playback from Journal is usually done with a single match
against the `TLOG_REC` Journal field. The `TLOG_REC` field contains a copy of
the `rec` field from the logged JSON data, which is a host-unique ID of the
recording. For example, to playback a recording which contains this message
(output with `journalctl -o verbose` and abbreviated):

    Mon 2018-01-22 10:51:48.463904 EET [s=87ea0a3f655a48cd80d7f49053860806;...
        _AUDIT_LOGINUID=1000
        _UID=1000
        _GID=1000
        _AUDIT_SESSION=2
        _BOOT_ID=12ca5b356065453fb50adfe57007658a
        _MACHINE_ID=2d8d017e2b1144cbbdd049a8a997911b
        _HOSTNAME=bard
        PRIORITY=6
        _TRANSPORT=journal
        _SYSTEMD_OWNER_UID=1000
        TLOG_REC=12ca5b356065453fb50adfe57007658a-306a-26f2910
        TLOG_USER=nkondras
        TLOG_SESSION=2
        TLOG_ID=1
        MESSAGE={"ver":"2.3","host":"bard","rec":"12ca5b356065453fb50adfe57007658a-306a-26f2910",...
        SYSLOG_IDENTIFIER=tlog-rec
        _PID=12394
        _COMM=tlog-rec
        _SOURCE_REALTIME_TIMESTAMP=1516611108463904

you can take the ID either from the `TLOG_REC` field value directly, or from
the `MESSAGE` field (from the JSON `rec` field). You can then playback the
whole recording like this:

    tlog-play -r journal -M TLOG_REC=12ca5b356065453fb50adfe57007658a-306a-26f2910

### Playing back ongoing recordings

By default, once `tlog-play` reaches the last message a recording has so far,
it exits. However, it can be made to poll for new messages appearing with the
`-f/--follow` option, which is useful for playing back ongoing recordings.

### Playback controls

`Tlog-play` accepts several command-line options affecting playback, including
`-s/--speed` for setting playback speed multiplier, `-g/--goto` for specifying
the location the playback should be fast-forwarded to, and `-p/--paused` for
starting playback in paused state.

Several control keys are also recognized during playback:

* SPACE or `p` for pause/resume,
* `{` and `}` for halving and doubling the playback speed,
* `.` for stepping through the recording (on pause or during playback)
* `G` for fast-forwarding to the end of the recording (useful with
  `--follow`), or to the specified timestamp (see `tlog-play(8)` for details),
* and `q` for quitting playback.

### Rate-limiting recording

Both `tlog-rec` and `tlog-rec-session` can be setup to limit the rate at which
recording's messages are logged. `Tlog-rec` accepts three options:
`--limit-rate=NUMBER`, `--limit-burst=NUMBER`, and `--limit-action=STRING`,
which specify rate limit in bytes per second, burst limit in bytes, and the
limit action (pass/delay/drop), respectively. The same parameters can be
changed using `limit.rate`, `limit.burst`, and `limit.action` configuration
parameters for both `tlog-rec` and `tlog-rec-session`.

The default `pass` limit action lets all the messages through unhindered,
effectively disabling rate-limiting. You can throttle logging, and slow down
the user's terminal I/O using the `delay` limit action. Finally, you can
simply drop the captured I/O, going above the rate and burst limits, using the
`drop` action. See `tlog-rec(8)`, `tlog-rec.conf(5)`, and
`tlog-rec-session.conf(5)` for details.

### Playing back partial recordings

By default `tlog-play` will terminate playback, if it notices out-of-order or
missing log messages. However, it is possible to make it ignore missing
messages with the `--lax` option for when you need to playback a partial
or a corrupted recording.

### Recording sessions of a user

Change the shell of the user to be recorded to `tlog-rec-session`:

    sudo usermod -s /usr/bin/tlog-rec-session <user>

Login as the user on a text terminal. By default the recorded terminal data
will be delivered to Journal (if tlog was built with Journal support) or to
syslog with facility "authpriv". In both cases default priority will be
"info". It will appear in Journal, if you use journald, or in
`/var/log/auth.log` on Debian-based systems, and in `/var/log/secure` on
Fedora and derived systems.

Customize `tlog-rec-session` configuration in
`/etc/tlog/tlog-rec-session.conf` as necessary (see `tlog-rec-session.conf(5)`
for details).

#### Automatically recording login sessions for users

Sample scripts have been made available in `/usr/share/doc/tlog/profile.d` that
provide an automatic method for recording sessions from users or groups
specified in `/etc/security/tlog.users`.

To use these scripts, simply copy them into `/etc/profile.d`.

A valid `tlog.users` file might look like the following:

```
# Log all actions by the 'root' user
root

# Log all actions by anyone in the 'admins' group
%admins
```

Note: Whitespace is **not** ignored.

#### Locale configuration issue on Fedora and RHEL

Fedora and RHEL (and some other distros) use an approach for configuring
system locale, where the login shell is responsible for reading the locale
configuration from a file (`/etc/locale.conf`) itself, instead of receiving it
through the environment variables as most programs do. Since `su` clears
environment when asked for imitation of a login shell (`su -`, or `su -l`),
the shell can only retrieve locale configuration from that file, in that case,
on these distros.

Because `tlog-rec-session` is not an actual shell and cannot read
`/etc/locale.conf` file itself, it will use libc routines to read the
environment, which will fall back to `ANSI_X3.4-1968` (ASCII) in these cases.
Since nowadays pure ASCII is rarely used, `tlog-rec-session` assumes that
locale environment was lost, assumes the actual encoding is UTF-8, and prints
a warning.

To work that around, you can implement the approach Debian and derived distros
use. I.e. use PAM's pam_env.so module to read and set the locale environment
variables before shell or `tlog-rec-session` starts. To accomplish that on
Fedora or RHEL, put this into the `/etc/pam.d/system-auth` file, along with
all other `session` lines:

    session     required      pam_env.so readenv=1 envfile=/etc/locale.conf

However, tlog only supports UTF-8 so far, so the above workaround only serves
to silent the fallback warning.

#### Configuring shell per-user using symlinks

You can create a symlink to `tlog-rec-sessions` containing the shell it should
start, in its name. E.g. if you create a symlink like this:

    sudo ln -s tlog-rec-session /usr/bin/tlog-rec-session-shell-bin-zsh

and execute `tlog-rec-session-shell-bin-zsh`, then `tlog-rec-session` will
start and execute `/bin/zsh` as the shell to record. See `tlog-rec-session(8)`
for details.

Such symlinks can then be assigned as login shells to certain users to have a
specific shell started for them, under recording.

#### Configuring recording in SSSD

SSSD starting with v1.16.0 allows configuring which users and/or groups should
be recorded (have `tlog-rec-session` started when they login), while also
preserving the original user shell. See `sssd-session-recording(5)`.

### Recording sessions to Elasticsearch

Rsyslog can be set up to deliver tlog messages to Elasticsearch. First of
all, increase the maximum message size to be 1k more than the
`tlog-rec-session` payload. The default payload is 2kB, so the `rsyslog`
maximum message size needs to be "3k" if the defaults are used:

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
needs to be added. If you installed v4 or later tlog RPM package, or set up
`tlog-rec-session` as SUID/SGID to a dedicated user yourself, then the rule
can use that user ID to filter genuine tlog messages securely.

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

The `<TLOG_UID>` above should be replaced with the UID your `tlog-rec-session`
runs as.

Otherwise you'll need to filter by something else. For example the program
name (`ident` argument to syslog(3)), which tlog specifies as `tlog`. In that
case the condition could be:

    if $programname == "tlog-rec-session" then {
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

Here is a complete example of a rule matching messages arriving from
`tlog-rec-session` running as user with UID 123, delivered from journald. It
sends them to Elasticsearch running on localhost with default port, puts them
into `tlog-rsyslog` index with type `tlog`, using bulk interface, stores them
in `/var/log/tlog.log` file, and then stops processing, not letting them get
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

Currently `tlog-play` functionality is limited. It doesn't provide a way to
rewind playback, only to fast-forward. It polls files, Journal and
Elasticsearch for new messages, instead of asking to be updated when they
appear, which impairs performance. Even though the messages contain recorded
terminal window (re)sizes, it doesn't resize its own terminal to fit the
output better. Further development will be addressing these.

### Contributing
Please read the [Contributing Guidelines](CONTRIBUTING.md) for more details
on the process for submitting pull requests.

[session_recording]: http://scribery.github.io/
[log_format]: doc/log_format.md
[elasticsearch]: https://www.elastic.co/products/elasticsearch
[releases]: https://github.com/Scribery/tlog/releases
[query_string_syntax]: https://www.elastic.co/guide/en/elasticsearch/reference/current/query-dsl-query-string-query.html#query-string-syntax
