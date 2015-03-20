
COMPILER = gcc
DIR_PERM=0755
FILE_PERM=0644

LIBPURPLE_CFLAGS += $(shell pkg-config --cflags glib-2.0 json-glib-1.0 purple)
LIBPURPLE_LIBS += $(shell pkg-config --libs glib-2.0 json-glib-1.0 purple)
PLUGIN_DIR_PURPLE=$(shell pkg-config --variable=plugindir purple)
DATA_ROOT_DIR_PURPLE=$(shell pkg-config --variable=datarootdir purple)

PRPL_NAME=libskypeweb.so
PRPL_LIBNAME=${PRPL_NAME}

SKYPEWEB_SOURCES = \
	skypeweb_connection.c \
	skypeweb_contacts.c \
	skypeweb_login.c \
	skypeweb_messages.c \
	skypeweb_util.c \
	libskypeweb.c 

.PHONY:	all clean install
all: ${PRPL_NAME}
install:
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(PLUGIN_DIR_PURPLE)
	install -m $(FILE_PERM) $(PRPL_LIBNAME) $(DESTDIR)$(PLUGIN_DIR_PURPLE)/$(PRPL_NAME)
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16
	install -m $(FILE_PERM) icons/16/skype.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16/skype.png
	install -m $(FILE_PERM) icons/16/skypeout.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16/skypeout.png
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22
	install -m $(FILE_PERM) icons/22/skype.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22/skype.png
	install -m $(FILE_PERM) icons/22/skypeout.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22/skypeout.png
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48
	install -m $(FILE_PERM) icons/48/skype.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48/skype.png
	install -m $(FILE_PERM) icons/48/skypeout.png $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48/skypeout.png
	mkdir -m $(DIR_PERM) -p $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/emotes/skype
	install -m $(FILE_PERM) theme $(DESTDIR)$(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/emotes/skype/theme
clean:
	rm -f libskypeweb.so

${PRPL_NAME}: ${SKYPEWEB_SOURCES}
	${COMPILER} -Wall -I. -g -O2 -fPIC -pipe ${SKYPEWEB_SOURCES} -o $@ ${LIBPURPLE_CFLAGS} ${LIBPURPLE_LIBS} -shared
