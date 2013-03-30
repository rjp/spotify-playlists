#! /bin/sh
CC=${CC:-gcc}
DEPS="appkey.o playlist-xspf.o pl-queue.o"
redo-ifchange $DEPS
${CC} -o px $DEPS -g -Wall -framework libspotify
