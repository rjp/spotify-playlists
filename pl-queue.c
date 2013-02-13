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

typedef SLIST_HEAD(pl_queue_t, pl_queue_entry) pl_queue;

static pl_queue playlists_pending;
static pl_queue playlists_working;

struct pl_queue_entry {
    sp_playlist *pl; /* our queued playlist */
    SLIST_ENTRY(pl_queue_entry) entries;
};

void
init_playlist_queue(pl_queue *playlist) {
    SLIST_INIT(playlist);
}

void
init_playlist_queues() {
    init_playlist_queue(&playlists_pending);
    init_playlist_queue(&playlists_working);
}

void
queue_playlist(sp_playlist *pl, pl_queue *playlist) {
    /* and a partridge in a pear tree */
    struct pl_queue_entry *t = (struct pl_queue_entry *)malloc(sizeof(struct pl_queue_entry));
    t->pl = pl;
    SLIST_INSERT_HEAD(playlist, t, entries);
}

void
queue_pending(sp_playlist *pl) {
    queue_playlist(pl, &playlists_pending);
}

void
queue_working(sp_playlist *pl) {
    queue_playlist(pl, &playlists_working);
}

sp_playlist *
dequeue_playlist(pl_queue *playlist) {
    sp_playlist *r_pl;
    struct pl_queue_entry *t;

    /* empty list returns NULL on dequeue */
    if (SLIST_EMPTY(playlist)) {
       return NULL;
    }

    t = SLIST_FIRST(playlist);
    r_pl = t->pl; /* save our playlist pointer */

    /* no need for the queue entry any more */
    SLIST_REMOVE_HEAD(playlist, entries);
    free(t);

    return r_pl;
}

sp_playlist *
dequeue_pending(void) {
    return dequeue_playlist(&playlists_pending);
}

void
remove_working(sp_playlist *pl) {
    // remove this playlist from the work list somehow
    struct pl_queue_entry *np;

    SLIST_FOREACH(np, &playlists_working, entries) {
        if (np->pl == pl) {
            fprintf(stderr, "W-  %s\n", sp_playlist_name(np->pl));
            SLIST_REMOVE(&playlists_working, np, pl_queue_entry, entries);
        } else {
            fprintf(stderr, "W=  %s\n", sp_playlist_name(np->pl));
        }
    }
}

int
still_working(void) {
    return ! SLIST_EMPTY(&playlists_working);
}

