#!/bin/bash
set -eux

cd /tmp && wget https://github.com/Scribery/tlog/releases/download/v$CURRENT_VERSION/tlog-$CURRENT_VERSION.tar.gz
tar xf tlog-$CURRENT_VERSION.tar.gz && cd tlog-6
# 
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make -j $(nproc --all)
make install DESTDIR=/tmp/tlog
cp /tmp/tlog.ctl /tmp/tlog/
# 
sed -i "s/CURRENT_VERSION/${CURRENT_VERSION}/g" /tmp/tlog/tlog.ctl
sed -i "s/REVISION/${REVISION}/g" /tmp/tlog/tlog.ctl
sed -i "s/DEPENDS/${DEPENDS}/g" /tmp/tlog/tlog.ctl
# 
cd /tmp/tlog
cp /templates/tlog-rec-session.conf etc/tlog/
mkdir usr/share/tlog/rsyslog.d &&  cp /templates/rsyslog-tlog-rec-session.conf usr/share/tlog/rsyslog.d/
mkdir usr/share/tlog/logrotate.d &&  cp /templates/log-rotate-tlog usr/share/tlog/logrotate.d/
equivs-build tlog.ctl
ls -l 
mv "/tmp/tlog/tlog_${CURRENT_VERSION}-${REVISION}_amd64.deb" "/packages/tlog_${CURRENT_VERSION}-${PKG_RELEASE}_amd64.deb"
