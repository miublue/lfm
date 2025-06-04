USECOLOR ?= 1

ifdef USECOLOR
	CFLAGS := -D_USE_COLOR
endif

all:
	tcc -O2 -o lfm *.c -lncurses ${CFLAGS}

install: all
	install lfm /usr/local/bin

