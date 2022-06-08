# Makefile to make the mkrom tool and associated libraries.
CC=gcc
AR=ar

CFLAGS=-Wall -fPIC -fvisibility=hidden
INCLUDE=-I/usr/include/lua5.3

AUTO_GEN= .lua_src.c

LIB_SRC= romfs.c \
         luaromfs.c \
         sha256.c \
				 aes.c

LIB_HDR= romfs.h \
         luaromfs.h \
         sha256.h \
         aes.h

BIN_SRC= mkrom.c \
         sha256.c \
				 aes.c

BIN_HDR= sha256.h \
         aes.h

all: mkrom libluaromfs.a example

mkrom: Makefile ${BIN_SRC} ${BIN_HDR}
	${CC} ${CFLAGS} -o $@ ${BIN_SRC} -lz

libluaromfs.a: Makefile ${AUTO_GEN} ${LIB_SRC} ${LIB_HDR}
	${CC} ${CFLAGS} ${INCLUDE} -c ${LIB_SRC}
	${AR} rcs libluaromfs.a $(patsubst %.c,%.o,${LIB_SRC})

.PHONY: always

.lua_src.c: mkrom always
	./mkrom -c lua_src -s -x lua_src/ lua_src/ .lua_src.c

.PHONY: clean distclean example

example: mkrom libluaromfs.a
	cd example && make && ./example

clean:
	rm -f *.o ${AUTO_GEN}

distclean: clean
	rm -f mkrom libluaromfs.a

