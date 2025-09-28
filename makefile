CC = tcc
USECOLOR = 1
BINPATH = /usr/local/bin
CFLAGS = 

ifdef USECOLOR
	CFLAGS += -D_USE_COLOR
endif

all:
	${CC} -O2 -o lfm *.c -lncurses ${CFLAGS}

install: all
	mkdir -p ${BINPATH}
	install lfm ${BINPATH}

uninstall:
	rm ${BINPATH}/lfm

