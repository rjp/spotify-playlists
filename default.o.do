redo-ifchange $2.c
gcc -Wall -g -MD -MF $2.d -c -o $3 $2.c
read DEPS <$2.d
redo-ifchange ${DEPS#*:}
