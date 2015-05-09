%define debug_package %{nil}
%define plugin_name skypeweb

Name: purple-%{plugin_name}
Version: 0.1
Release: 1
Summary: Adds support for Skype to Pidgin
Group: Applications/Productivity
License: GPLv3
URL: https://github.com/EionRobb/skype4pidgin
Source0: %{name}.tar.gz

BuildRequires: glib2-devel
BuildRequires: libpurple-devel
BuildRequires: json-glib-devel
BuildRequires: gcc
Requires: libpurple
Requires: json-glib

%package -n pidgin-%{plugin_name}
Summary: Adds pixmaps, icons and smileys for Skype protocol.
Requires: %{name}
Requires: pidgin

%description
Adds support for Skype to Pidgin, Adium, Finch and other libpurple 
based messengers.

%description -n pidgin-%{plugin_name}
Adds pixmaps, icons and smileys for Skype protocol inplemented by libskypeweb.

%prep
%setup -c

%build
make

%install
make install DESTDIR=%{buildroot}

%files
%{_libdir}/purple-2/libskypeweb.so

%files -n pidgin-%{plugin_name}
%{_datadir}/pixmaps/pidgin/protocols/16/skype.png
%{_datadir}/pixmaps/pidgin/protocols/16/skypeout.png
%{_datadir}/pixmaps/pidgin/protocols/22/skype.png
%{_datadir}/pixmaps/pidgin/protocols/22/skypeout.png
%{_datadir}/pixmaps/pidgin/protocols/48/skype.png
%{_datadir}/pixmaps/pidgin/protocols/48/skypeout.png
%{_datadir}/pixmaps/pidgin/emotes/skype/theme

%changelog
* Sat May 09 2015 V1TSK <vitaly@easycoding.org> - 0.1-2
- Separated packages. Now can be used with other libpurple-based clients without Pidgin being installed.

* Mon Mar 16 2015 V1TSK <vitaly@easycoding.org> - 0.1-1
- Created first RPM spec for Fedora/openSUSE.
