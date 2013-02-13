DEPS="appkey.o playlist-xspf.o pl-queue.o"
redo-ifchange $DEPS
gcc -o px $DEPS -g -Wall -framework libspotify
