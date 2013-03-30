#! /bin/sh
CC=${CC:-gcc}
DEPS="appkey.o playlist-xspf.o pl-queue.o"
redo-ifchange $DEPS

case "$(uname)" in
    *Darwin*) LIBS="-framework libspotify" ;;
    *) LIBS="-L/usr/local/lib -lspotify" ;;
esac

${CC} -o $3 $DEPS -g -Wall $LIBS
