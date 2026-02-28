CC = tcc
USECOLOR = 1
PREFIX = /usr/local
CFLAGS = 

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

ifdef USECOLOR
	CFLAGS += -D_USE_COLOR
endif

all:
	${CC} -O2 -o lfm *.c -lncurses ${CFLAGS}

install: all
	mkdir -p ${PREFIX}/bin
	install -s lfm ${PREFIX}/bin

uninstall:
	rm ${PREFIX}/bin/lfm

.PHONY: all install uninstall

