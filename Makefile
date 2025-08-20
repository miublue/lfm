USECOLOR ?= 1
USEMTM ?= 1
CFLAGS ?= 

ifdef USECOLOR
	CFLAGS += -D_USE_COLOR
endif

ifdef USEMTM
	CFLAGS += -D_USE_MTM
endif

all:
	tcc -O2 -o lfm *.c -lncurses ${CFLAGS}

install: all
	install lfm /usr/local/bin

uninstall:
	rm /usr/local/bin/lfm

