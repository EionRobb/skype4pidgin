
#Customisable stuff here
LINUX32_COMPILER = i686-pc-linux-gnu-gcc
LINUX64_COMPILER = x86_64-pc-linux-gnu-gcc
WIN32_COMPILER = /usr/bin/i586-mingw32-gcc
LINUX_ARM_COMPILER = arm-none-linux-gnueabi-gcc

LIBPURPLE_CFLAGS = -I/usr/include/libpurple -DPURPLE_PLUGINS -DENABLE_NLS
GLIB_CFLAGS = `pkg-config --cflags glib-2.0` -I/usr/include
DBUS_CFLAGS = -DSKYPE_DBUS `pkg-config --cflags dbus-1`
WIN32_DEV_DIR = /root/pidgin/win32-dev
WIN32_PIDGIN_DIR = /root/pidgin/pidgin-2.6.1
WIN32_CFLAGS = -DPURPLE_PLUGINS -DENABLE_NLS -I${WIN32_DEV_DIR}/gtk_2_0/include/glib-2.0 -I${WIN32_PIDGIN_DIR}/libpurple/win32 -I${WIN32_PIDGIN_DIR}/libpurple -I${WIN32_DEV_DIR}/gtk_2_0/include -I${WIN32_DEV_DIR}/gtk_2_0/include/glib-2.0 -I${WIN32_DEV_DIR}/gtk_2_0/lib/glib-2.0/include
WIN32_LIBS = -L${WIN32_DEV_DIR}/gtk_2_0/lib -L${WIN32_PIDGIN_DIR}/libpurple -lglib-2.0 -lgobject-2.0 -lgthread-2.0 -lintl -lpurple
XUL_LIBS = -I/usr/lib/xulrunner/include xpcomModule.cpp -I/usr/include/nspr
X11_LIBS = `pkg-config --libs x11`

VV_CFLAGS = -I/usr/include/gstreamer-0.10 -DUSE_VV -I/usr/include/libxml2
WIN32_VV_CFLAGS = -I${WIN32_DEV_DIR}/libxml2/include -I${WIN32_DEV_DIR}/gstreamer-0.10/include -I${WIN32_DEV_DIR}/gstreamer-0.10/include/gstreamer-0.10 

DEB_PACKAGE_DIR = /root/skypeplugin

LOCALES = $(patsubst %.po, %.mo, $(wildcard po/*.po))

#Standard stuff here
.PHONY:	all clean allarch install locales uninstall

.DEPENDS: libskype.c skype_messaging.c skype_events.c debug.c

allarch:	skype4pidgin.deb skype4pidgin-installer.exe libskype_dbus.so libskype_dbus64.so libskypearm.so

#By default, 'make' compiles X11 and DBus versions on local platform
all: .DEPENDS skype_messaging_x11.c skype_messaging_dbus.c
	gcc ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -pipe libskype.c -o libskype.so -shared -fPIC -DPIC ${X11_LIBS}
	gcc ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -pipe libskype.c -o libskype_dbus.so -shared -fPIC -DPIC ${DBUS_CFLAGS}

install: locales
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/emotes/skype
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48
	mkdir -p $(DESTDIR)/usr/lib/purple-2
	install -m 664 theme $(DESTDIR)/usr/share/pixmaps/pidgin/emotes/skype/
	install -m 664 icons/16/skypeout.png icons/16/skype.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16
	install -m 664 icons/22/skypeout.png icons/22/skype.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22
	install -m 664 icons/48/skypeout.png icons/48/skype.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48
	install -m 664 libskype_dbus.so libskype.so $(DESTDIR)/usr/lib/purple-2/

uninstall:
	rm -rf $(DESTDIR)/usr/share/pixmaps/pidgin/emotes/skype
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16/skypeout.png
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16/skype.png
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22/skypeout.png
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22/skype.png
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48/skypeout.png
	rm -rf  $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48/skype.png
	rm -rf $(DESTDIR)/usr/lib/purple-2/libskype.so
	rm -rf $(DESTDIR)/usr/lib/purple-2/libskype_dbus.so

clean:
	rm -f libskype.so libskype64.so libskype_dbus.so libskype_dbus64.so libskypearm.so libskype.dll skype4pidgin.deb skype4pidgin-installer.exe po/*.mo

libskypenet.so:  .DEPENDS skype_messaging_network.c
	${LINUX32_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -march=athlon-xp -O2 -pipe libskype.c -o libskypenet.so -shared -fPIC -DPIC -DSKYPENET

libskype.so: .DEPENDS skype_messaging_x11.c
	${LINUX32_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -march=athlon-xp -pipe libskype.c -o libskype.so -shared -fPIC -DPIC

libskype64.so: .DEPENDS skype_messaging_x11.c
	${LINUX64_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -m32 -m64 -O2 -pipe libskype.c -o libskype64.so -shared -fPIC -DPIC -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libskypenet64.so: .DEPENDS skype_messaging_network.c
	${LINUX64_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -m32 -m64 -O2 -pipe libskype.c -o libskypenet64.so -shared -fPIC -DPIC -DSKYPENET -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libskypearm.so: .DEPENDS skype_messaging_x11.c
	${LINUX_ARM_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -O2 -pipe libskype.c -o libskypearm.so -shared -fPIC -DPIC

libskype_dbus.so: .DEPENDS skype_messaging_dbus.c
	${LINUX32_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -march=athlon-xp -pipe libskype.c -o libskype_dbus.so -shared -fPIC -DPIC ${DBUS_CFLAGS}

libskype_dbus64.so: .DEPENDS skype_messaging_dbus.c
	${LINUX64_COMPILER} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -m64 -pipe libskype.c -o libskype_dbus64.so -shared -fPIC -DPIC ${DBUS_CFLAGS} -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libskype.dll: .DEPENDS skype_messaging_win32.c
	${WIN32_COMPILER} -Wall -I. -g -O2 -pipe libskype.c -o libskype.dll -shared -mno-cygwin ${WIN32_CFLAGS} ${WIN32_LIBS} -Wl,--strip-all
	upx libskype.dll

libskype-xul.dll: .DEPENDS skype_messaging_win32.c
	${WIN32_COMPILER} -Wall -I. -g -O2 -pipe libskype.c ${XUL_LIBS} -o libskype-xul.dll -shared -mno-cygwin ${WIN32_CFLAGS} ${WIN32_LIBS} -Wl,--strip-all

libskypenet.dll: .DEPENDS skype_messaging_network.c
	${WIN32_COMPILER} -Wall -I. -g -O2 -pipe libskype.c -o libskypenet.dll -shared -mno-cygwin ${WIN32_CFLAGS} ${WIN32_LIBS} -DSKYPENET -Wl,--strip-all
	upx libskypenet.dll

libskype-debug.dll: .DEPENDS skype_messaging_win32.c
	${WIN32_COMPILER} -Wall -I. -g -O2 -pipe libskype.c -o libskype-debug.dll -shared -mno-cygwin ${WIN32_CFLAGS} ${WIN32_LIBS}

libskypenet-debug.dll: .DEPENDS skype_messaging_network.c
	${WIN32_COMPILER} -Wall -I. -g -O2 -pipe libskype.c -o libskypenet-debug.dll -shared -mno-cygwin ${WIN32_CFLAGS} ${WIN32_LIBS} -DSKYPENET

po/%.mo: po/%.po
	msgfmt -cf -o $@ $<

locales:	${LOCALES}

skype4pidgin-installer.exe: libskype.dll locales
	date=`date +%d-%b-%Y` && sed "s/PRODUCT_VERSION \"[-a-z0-9A-Z]*\"/PRODUCT_VERSION \"$$date\"/" -i skype4pidgin.nsi
	echo "Making .exe package"
	makensis skype4pidgin.nsi > /dev/null

skype4pidgin.deb: locales libskype.so libskype64.so libskype_dbus.so libskype_dbus64.so libskypearm.so
	rm ${DEB_PACKAGE_DIR}/usr/lib/purple-2/libskype*.so
	cp libskype.so libskype64.so libskype_dbus.so libskype_dbus64.so libskypearm.so ${DEB_PACKAGE_DIR}/usr/lib/purple-2/
	date=`date +%F` && sed "s/Version: [-a-z0-9A-Z]*/Version: $$date/" -i ${DEB_PACKAGE_DIR}/DEBIAN/control
	echo "Making .deb package"
	dpkg-deb --build ${DEB_PACKAGE_DIR} skype4pidgin.deb > /dev/null
	
