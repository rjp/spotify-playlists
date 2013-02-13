#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/queue.h>

/* only needed for typedefs */
#include <libspotify/api.h>

SLIST_HEAD(pl_queue_t, pl_queue_entry) playlists;

struct pl_queue_entry {
    sp_playlist *pl; /* our queued playlist */
    SLIST_ENTRY(pl_queue_entry) pending;
};

void
init_playlist_queue(void) {
    SLIST_INIT(&playlists);
}

void
queue_playlist(sp_playlist *pl) {
    /* and a partridge in a pear tree */
    struct pl_queue_entry *t = (struct pl_queue_entry *)malloc(sizeof(struct pl_queue_entry));
    t->pl = pl;
    SLIST_INSERT_HEAD(&playlists, t, pending);
}

sp_playlist *
dequeue_playlist(void) {
    sp_playlist *r_pl;
    struct pl_queue_entry *t;

    /* empty list returns NULL on dequeue */
    if (SLIST_EMPTY(&playlists)) {
       return NULL;
    }

    t = SLIST_FIRST(&playlists);
    r_pl = t->pl; /* save our playlist pointer */

    /* no need for the queue entry any more */
    SLIST_REMOVE_HEAD(&playlists, pending);
    free(t);

    return r_pl;
}
