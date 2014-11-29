
COMPILER = gcc

LIBPURPLE_CFLAGS += $(shell pkg-config --cflags glib-2.0 json-glib-1.0 purple)
LIBPURPLE_LIBS += $(shell pkg-config --libs glib-2.0 json-glib-1.0 purple)

SKYPEWEB_SOURCES = \
	skypeweb_connection.c \
	skypeweb_contacts.c \
	skypeweb_login.c \
	skypeweb_messages.c \
	skypeweb_util.c \
	libskypeweb.c 

.PHONY:	all clean install
all: libskypeweb.so
clean:
	rm -f libskypeweb.so

libskypeweb.so: ${SKYPEWEB_SOURCES}
	${COMPILER} -Wall -I. -g -O2 -fPIC -pipe ${SKYPEWEB_SOURCES} -o $@ ${LIBPURPLE_CFLAGS} ${LIBPURPLE_LIBS} -shared
