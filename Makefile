all:
	tcc -O2 -o lfm *.c -lncurses -D_USE_COLOR

install: all
	install lfm /usr/local/bin

