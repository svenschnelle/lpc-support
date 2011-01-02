CC=/cygdrive/c/mingw/bin/gcc
CFLAGS=-Wextra -Wall -O2 -Wno-unused -ggdb

all: LPC.dll

LPC.dll:	LPC.c LPC.h Makefile
		$(CC) $(CFLAGS) -shared -o $@ $<

install:	LPC.dll LPC.tla LPC_cycletype.tsf
		mkdir -p /cygdrive/c/Program\ Files/TLA\ 700/Supports/LPC/
		cp -Rva $^ /cygdrive/c/Program\ Files/TLA\ 700/Supports/LPC/
clean:

		rm -f LPC.dll