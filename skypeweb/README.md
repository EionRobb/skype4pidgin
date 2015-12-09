SkypeWeb Plugin for Pidgin
==========================

Adds a "Skype (HTTP)" protocol to the accounts list.  Requires libjson-glib.  GPLv3 Licenced.

Download latest releases from [here](https://github.com/EionRobb/skype4pidgin/releases)

Windows
-------
Windows users can download latest builds from [libskypeweb-debug.dll](http://eion.robbmob.com/libskypeweb-debug.dll) or [pidgin-skypeweb-installer.exe](http://eion.robbmob.com/pidgin-skypeweb-installer.exe)

The plugin requires libjson-glib which is part of the installer exe or can be downloaded [from github](https://github.com/EionRobb/skype4pidgin/raw/master/skypeweb/libjson-glib-1.0.dll) and copied to the Program Files\Pidgin folder (not the plugins subfolder)


Compiling
---------
Requires devel headers/libs for libpurple and libjson-glib [libglib2.0-dev, libjson-glib-dev and libpurple-dev]
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
	sudo yum install rpm-build gcc json-glib-devel libpurple-devel zlib-devel make automake glib2-devel -y
	mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
	wget https://raw.githubusercontent.com/EionRobb/skype4pidgin/master/skypeweb/purple-skypeweb.spec \
		-O ~/rpmbuild/SPECS/purple-skypeweb.spec
	wget https://github.com/EionRobb/skype4pidgin/archive/master.tar.gz \
		-O ~/rpmbuild/SOURCES/skype4pidgin-1.0.tar.gz
	rpmbuild -ba  ~/rpmbuild/SPECS/purple-skypeweb.spec
```
The result can be found in ``~/rpmbuild/RPMS/`uname -m`/`` directory.


Building DEB package for Debian/Ubuntu/Mint
---------
Requires devel headers/libs for libpurple and json-glib, gcc compiler and cmake
```
	sudo apt install libpurple-dev libjson-glib-dev cmake gcc
	git clone git://github.com/EionRobb/skype4pidgin.git
	cd skype4pidgin/skypeweb
	mkdir build
	cd build
	cmake ..
	cpack
```
To install do:
```
	sudo dpkg -i skypeweb-0.1.0-Linux.deb
```

Show your appreciation
----------------------
Did this plugin make your life happier?  [Send me $1](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=D33N5RV7FEXZU) to say thanks!
