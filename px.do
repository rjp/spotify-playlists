#! /bin/sh
CC=${CC:-gcc}
DEPS="appkey.o playlist-xspf.o pl-queue.o"
redo-ifchange $DEPS

case "$(uname)" in
    *Darwin*) LIBS="-framework libspotify" ;;
    *) LIBS="-llibspotify" ;;
esac

${CC} -o px $DEPS -g -Wall $LIBS
