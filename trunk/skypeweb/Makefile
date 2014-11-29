
COMPILER = gcc

LIBPURPLE_CFLAGS += $(shell pkg-config --cflags glib-2.0 json-glib-1.0 purple nss gnome-keyring-1)
LIBPURPLE_LIBS += $(shell pkg-config --libs glib-2.0 json-glib-1.0 purple nss)

STEAM_SOURCES = \
	steam_connection.c \
	libsteam.c 

.PHONY:	all clean install
all: libsteam.so
clean:
	rm -f libsteam.so

libsteam.so: ${STEAM_SOURCES}
	${COMPILER} -Wall -I. -g -O2 -fPIC -pipe ${STEAM_SOURCES} -o $@ ${LIBPURPLE_CFLAGS} ${LIBPURPLE_LIBS} -shared
