LIBS=$(shell pkg-config fuse --libs)
CFLAGS=$(shell pkg-config fuse --cflags)
install:
	gcc -o jkfs jkfs.c ${CFLAGS} -g ${LIBS}
hinstall:
	gcc -o hjkfs hjkfs.c ${CFLAGS} -g ${LIBS}
sinstall:
	gcc -o sjkfs sjkfs.c ${CFLAGS} -g ${LIBS}

