DEPS="appkey.o playlist-xspf.o"
redo-ifchange $DEPS
gcc -o $3 $DEPS -g -Wall -framework libspotify
