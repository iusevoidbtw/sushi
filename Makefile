.POSIX:

CC     = cc
SRC    = sushi.c

PREFIX = /usr/local

WFLAGS = -Wall -Wextra -Wdeclaration-after-statement -Wshadow -Wpointer-arith -Wcast-align -Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -Wconversion -Wstrict-prototypes -Wdeprecated

CFLAGS = -std=c89 -pedantic -Os -g -Werror ${WFLAGS}

sushi: ${SRC}
	${CC} ${CFLAGS} -o sushi ${SRC}
install: sushi
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f sushi ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/sushi
uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/sushi
clean:
	rm -f sushi *.o
