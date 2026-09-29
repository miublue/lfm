CC = tcc
PREFIX = /usr/local
CFLAGS = -Wall -Werror -Wextra
USECOLOR = 1
USEMTM = 1

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

ifdef USECOLOR
	CFLAGS += -D_USE_COLOR
endif

all:
	${CC} -O2 -o lfm lfm.c -lncurses ${CFLAGS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s lfm ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/lfm

.PHONY: all install uninstall

