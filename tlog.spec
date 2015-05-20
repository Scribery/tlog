Name:       tlog
Version:    1
Release:    1%{?dist}
Summary:    Terminal I/O recording shell wrapper

License:    GPLv2+
Source:     %{name}-%{version}.tar.gz

%description
 Tlog is a terminal I/O recording program similar to "script", but used in
 place of a user's shell, starting the recording and executing the real user's
 shell afterwards. The recorded I/O can then be forwarded to a logging server
 in JSON format.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%doc
%{_defaultdocdir}/%{name}
%{_bindir}/tlog

%changelog
