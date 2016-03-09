Tlog
====

[![Build Status](https://travis-ci.org/Scribery/tlog.svg?branch=master)](https://travis-ci.org/Scribery/tlog)

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
recorded data to a storage service such as [ElasticSearch][elasticsearch],
where it can be searched and queried, and from where it can be played back.

`Tlog` is naturally split into two tools: `tlog-rec` and `tlog-play` - for
recording and playback respectively. `Tlog-rec` is intended to be the user's
login shell. It puts itself between the actual user's shell and the terminal
upon user login, logging everything that passes through. At the moment,
`tlog-play` can only playback recordings from ElasticSearch. However, other
sources are going to be implemented in future releases.

Building
--------

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

If you built `tlog` from source, you can install it with the usual `make
install`:

    sudo make install

Otherwise you can use the [release binary RPM packages][releases] and install
them with your favorite tool.

Usage
-----

Change the shell of the user to be recorded to `tlog-rec`:

    sudo chsh -s /usr/bin/tlog-rec <user>

Login as the user on a text terminal. The recorded terminal data will be
delivered to syslog with facility "authpriv" and priority "info", and may
appear in `/var/log/auth.log` on Debian-based systems, or in `/var/log/secure`
on Fedora and derived systems.

Customize `tlog-rec` configuration in `/etc/tlog/tlog-rec.conf` as necessary
(see `tlog-rec.conf(5)` for details).

`Rsyslog` can be set up to deliver `tlog` messages to ElasticSearch. First of
all increase the maximum message size to be 1k more than the `tlog-rec` payload.
The default payload is 2kB by default, so the `rsyslog` maximum message size
needs to be "3k" if the defaults are used:

    $MaxMessageSize 3k

The line above needs to be put above any network setup in `rsyslog.conf` (put
it at the top to be safe).

Then the ElasticSearch output module needs to be loaded:

    $ModLoad omelasticsearch

Before sending `tlog` messages to ElasticSearch they need to be reformatted
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

Finally, a rule to send messages originating from `tlog` to ElasticSearch
needs to be added:

    :programname, isequal, "tlog" action(name="tlog-elasticsearch"
                                         type="omelasticsearch"
                                         server="localhost"
                                         searchIndex="tlog"
                                         searchType="tlog"
                                         bulkmode="on"
                                         template="tlog")

Add the following rule immediately after the above if you want to also send
`tlog` messages to a dedicated file for debugging:

    &                             action(name="tlog-file"
                                         type="omfile"
                                         file="/var/log/tlog.log"
                                         fileCreateMode="0600"
                                         template="tlog")

Further, if you don't want `tlog` messages delivered anywhere else you can add
this right after any of those:

    & ~

If you'd like to exclude `tlog` messages from *any* other logs remember to put
these rules before any other rules in `rsyslog.conf`.

Note that the above setup would send any messages marked as originating from
program `tlog` to ElasticSearch. That can be easily forged, so **do not**
assume any of the delivered messages are actually authentic or came from the
user or host specified in them.

In the future `tlog` will run under a special user, which would help to
securely filter messages in rsyslog and increase the confidence of message
authenticity.

[session_recording]: http://spbnick.github.io/2015/10/26/open-source-session-recording.html
[log_format]: doc/log_format.md
[elasticsearch]: https://www.elastic.co/products/elasticsearch
[releases]: https://github.com/spbnick/tlog/releases
