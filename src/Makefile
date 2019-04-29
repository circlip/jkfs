LIBS=$(shell pkg-config fuse --libs)
CFLAGS=$(shell pkg-config fuse --cflags)
install:
	gcc -o jkfs jkfs.c ${CFLAGS} -g ${LIBS}
finstall:
	gcc -o fjkfs fjkfs.c ${CFLAGS} -g ${LIBS}
