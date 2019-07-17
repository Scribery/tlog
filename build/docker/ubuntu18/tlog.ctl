Section: misc
Priority: optional
Homepage: https://scribery.github.io/
Standards-Version: 3.9.2

Package: tlog
Version: CURRENT_VERSION-REVISION
Maintainer: metanovii <metanovii@gmail.com>
Architecture: amd64
Depends: DEPENDS
Files: usr/bin/tlog-rec-session /usr/bin
 usr/bin/tlog-play /usr/bin
 usr/bin/tlog-rec /usr/bin
 usr/lib/libtlog.a  /usr/lib
 usr/lib/libtlog.la  /usr/lib
 usr/lib/libtlog.so  /usr/lib
 usr/lib/libtlog.so.0  /usr/lib
 usr/lib/libtlog.so.0.0.0  /usr/lib
 etc/tlog/tlog-rec-session.conf /etc/tlog
 etc/tlog/tlog-play.conf /etc/tlog
 etc/tlog/tlog-rec.conf /etc/tlog
 usr/share/tlog/rsyslog.d/rsyslog-tlog-rec-session.conf /usr/share/tlog/rsyslog.d
 usr/share/tlog/logrotate.d/log-rotate-tlog /usr/share/tlog/logrotate.d
 usr/include/tlog/conf_origin.h /usr/include/tlog
 usr/include/tlog/delay.h /usr/include/tlog
 usr/include/tlog/errs.h /usr/include/tlog
 usr/include/tlog/es_json_reader.h /usr/include/tlog
 usr/include/tlog/fd_json_reader.h /usr/include/tlog
 usr/include/tlog/fd_json_writer.h /usr/include/tlog
 usr/include/tlog/grc.h /usr/include/tlog
 usr/include/tlog/journal_json_reader.h /usr/include/tlog
 usr/include/tlog/journal_json_writer.h /usr/include/tlog
 usr/include/tlog/journal_misc.h /usr/include/tlog
 usr/include/tlog/json_chunk.h /usr/include/tlog
 usr/include/tlog/json_dispatcher.h /usr/include/tlog
 usr/include/tlog/json_misc.h /usr/include/tlog
 usr/include/tlog/json_msg.h /usr/include/tlog
 usr/include/tlog/json_reader.h /usr/include/tlog
 usr/include/tlog/json_reader_type.h /usr/include/tlog
 usr/include/tlog/json_sink.h /usr/include/tlog
 usr/include/tlog/json_source.h /usr/include/tlog
 usr/include/tlog/json_stream.h /usr/include/tlog
 usr/include/tlog/json_writer.h /usr/include/tlog
 usr/include/tlog/json_writer_type.h /usr/include/tlog
 usr/include/tlog/mem_json_reader.h /usr/include/tlog
 usr/include/tlog/mem_json_writer.h /usr/include/tlog
 usr/include/tlog/misc.h /usr/include/tlog
 usr/include/tlog/pkt.h /usr/include/tlog
 usr/include/tlog/play_conf_cmd.h /usr/include/tlog
 usr/include/tlog/play_conf.h /usr/include/tlog
 usr/include/tlog/play_conf_validate.h /usr/include/tlog
 usr/include/tlog/play.h /usr/include/tlog
 usr/include/tlog/rc.h /usr/include/tlog
 usr/include/tlog/rec_conf_cmd.h /usr/include/tlog
 usr/include/tlog/rec_conf.h /usr/include/tlog
 usr/include/tlog/rec_conf_validate.h /usr/include/tlog
 usr/include/tlog/rec.h /usr/include/tlog
 usr/include/tlog/rec_item.h /usr/include/tlog
 usr/include/tlog/rec_session_conf_cmd.h /usr/include/tlog
 usr/include/tlog/rec_session_conf.h /usr/include/tlog
 usr/include/tlog/rec_session_conf_validate.h /usr/include/tlog
 usr/include/tlog/rl_json_writer.h /usr/include/tlog
 usr/include/tlog/session.h /usr/include/tlog
 usr/include/tlog/sink.h /usr/include/tlog
 usr/include/tlog/sink_type.h /usr/include/tlog
 usr/include/tlog/source.h /usr/include/tlog
 usr/include/tlog/source_type.h /usr/include/tlog
 usr/include/tlog/syslog_json_writer.h /usr/include/tlog
 usr/include/tlog/syslog_misc.h /usr/include/tlog
 usr/include/tlog/tap.h /usr/include/tlog
 usr/include/tlog/timespec.h /usr/include/tlog
 usr/include/tlog/timestr.h /usr/include/tlog
 usr/include/tlog/trx_act.h /usr/include/tlog
 usr/include/tlog/trx_basic.h /usr/include/tlog
 usr/include/tlog/trx_frame.h /usr/include/tlog
 usr/include/tlog/trx.h /usr/include/tlog
 usr/include/tlog/trx_iface.h /usr/include/tlog
 usr/include/tlog/trx_level.h /usr/include/tlog
 usr/include/tlog/trx_state.h /usr/include/tlog
 usr/include/tlog/tty_sink.h /usr/include/tlog
 usr/include/tlog/tty_source.h /usr/include/tlog
 usr/include/tlog/utf8.h /usr/include/tlog
 usr/share/doc/tlog/log_format.md /usr/share/doc/tlog
 usr/share/doc/tlog/mapping.json /usr/share/doc/tlog
 usr/share/doc/tlog/README.md /usr/share/doc/tlog
 usr/share/doc/tlog/schema.json /usr/share/doc/tlog
 usr/share/man/man5/tlog-play.conf.5 /usr/share/man/man5
 usr/share/man/man5/tlog-rec.conf.5 /usr/share/man/man5
 usr/share/man/man5/tlog-rec-session.conf.5 /usr/share/man/man5
 usr/share/man/man8/tlog-play.8 /usr/share/man/man8
 usr/share/man/man8/tlog-rec.8 /usr/share/man/man8
 usr/share/man/man8/tlog-rec-session.8 /usr/share/man/man8
 usr/share/tlog/tlog-play.default.conf /usr/share/tlog
 usr/share/tlog/tlog-rec.default.conf /usr/share/tlog
 usr/share/tlog/tlog-rec-session.default.conf /usr/share/tlog
Description: Recorder tty session
File: postinst
 #!/bin/sh -e
 echo "Install doooone"
 echo "please read docs https://scribery.github.io/"
 if [ ! -d /var/run/tlog ]; then mkdir /var/run/tlog && chmod 777 /var/run/tlog ; fi
 cp /usr/share/tlog/rsyslog.d/rsyslog-tlog-rec-session.conf /etc/rsyslog.d/
 cp /usr/share/tlog/logrotate.d/log-rotate-tlog /etc/logrotate.d/
 systemctl restart rsyslog > /dev/null || true

