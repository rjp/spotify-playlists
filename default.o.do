#! /bin/sh
redo-ifchange $2.c
CC=${CC:-gcc}
CFLAGS="-wall -g -MD -MF $2.d"
${CC} ${CFLAGS} -c -o $3 $2.c
read DEPS <$2.d
redo-ifchange ${DEPS#*:}
