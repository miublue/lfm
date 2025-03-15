all:
	tcc -O2 -o lfm *.c -lncurses

install: all
	install lfm /usr/local/bin

