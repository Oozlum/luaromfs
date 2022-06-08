CC=gcc

CFLAGS=-g -Wall -fPIC
INCLUDES=-I/usr/include/lua5.3 -I..
LDFLAGS=-L/usr/lib/lua5.3 -L../

AUTOGEN= .internal_rom_src.c rom.bin

# This key will be used to encrypt the ROM binary that will be loaded dynamically by Lua.
ROM_KEY=MySecretKey

all: example

.PHONY: always

# INTERNAL_KEY is used to encrypt the compiled-in ROM file. It is automatically generated
# each time Make is run. Such a key can be useful where the key is purely mechanistic and
# does not need to be remembered and has the side-effect of avoiding version mismatch
# issues by guaranteeing that a ROM file can be loaded only by a specific binary build.
#
.internal_rom_src.c: INTERNAL_KEY=$(shell head /dev/urandom | LC_ALL=C tr -dc A-Za-z0-9 | head -c 13)
.internal_rom_src.c: always
	../mkrom -e "${INTERNAL_KEY}" -c rom -p -s -x internal_rom_src/ internal_rom_src/ .internal_rom_src.c

rom.bin: always
	../mkrom -e "${ROM_KEY}" -x rom_bin_src/ rom_bin_src/ rom.bin

example: Makefile main.c .internal_rom_src.c rom.bin
	${CC} ${CFLAGS} ${INCLUDES} -o $@ main.c ${LDFLAGS} -lluaromfs -lz -llua

.PHONY: clean distclean

clean:
	rm -f *.o ${AUTOGEN}

distclean: clean
	rm -f example

