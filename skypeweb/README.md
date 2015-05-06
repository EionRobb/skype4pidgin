SkypeWeb Plugin for Pidgin
==========================

Adds a "Skype (HTTP)" protocol to the accounts list.  Requires libjson-glib.  GPLv3 Licenced.

Until I get MSN/Live logins going there won't be any releases on GitHub.

Windows
-------
Windows users can download latest builds from [libskypeweb-debug.dll](http://eion.robbmob.com/libskypeweb-debug.dll) or [pidgin-skypeweb-installer.exe](http://eion.robbmob.com/pidgin-skypeweb-installer.exe)

The plugin requires libjson-glib which is part of the installer exe or can be downloaded [from github](https://github.com/EionRobb/skype4pidgin/raw/master/skypeweb/libjson-glib-1.0.dll) and copied to the Program Files\Pidgin folder (not the plugins subfolder)


Compiling
---------
Requires devel headers/libs for libpurple and libjson-glib
```
	git clone git://github.com/EionRobb/skype4pidgin.git
	cd skype4pidgin/skypeweb
	make
	sudo make install
```

Building RPM package for Fedora/openSUSE/CentOS/RHEL
---------
Requires devel headers/libs for libpurple and json-glib, gcc compiler and rpmbuild tool
```
	sudo yum install git rpm-build gcc json-glib-devel libpurple-devel pidgin-devel
	cd skype4pidgin/skypeweb
	tar -czf purple-skypeweb-0.1.tar.gz *
	rpmbuild -tb purple-skypeweb-0.1.tar.gz
```
The result can be found in ``~/rpmbuild/RPMS/`uname -m`/`` directory.
