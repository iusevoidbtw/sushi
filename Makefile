.POSIX:

CC     = cc
OUT    = sushi
SRC    = sushi.c

WFLAGS = -Wall -Wextra -Wdeclaration-after-statement -Wshadow -Wpointer-arith -Wcast-align -Wcast-qual -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -Wconversion -Wstrict-prototypes -Wdeprecated

CFLAGS = -std=c89 -pedantic -Os -g -Werror ${WFLAGS}

${OUT}: ${SRC}
	${CC} ${CFLAGS} -o ${OUT} ${SRC}
clean:
	rm -f ${OUT} *.o
