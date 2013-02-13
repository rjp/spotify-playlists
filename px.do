DEPS="appkey.o playlist-xspf.o pl-queue.o"
redo-ifchange $DEPS
gcc -o $3 $DEPS -g -Wall -framework libspotify
