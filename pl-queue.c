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

enum { HEAD, TAIL };

typedef STAILQ_HEAD(pl_queue_t, pl_queue_entry) pl_queue;

static pl_queue playlists_pending;
static pl_queue playlists_working;

struct pl_queue_entry {
    sp_playlist *pl; /* our queued playlist */
    STAILQ_ENTRY(pl_queue_entry) entries;
};

void
init_playlist_queue(pl_queue *playlist) {
    STAILQ_INIT(playlist);
}

void
init_playlist_queues() {
    init_playlist_queue(&playlists_pending);
    init_playlist_queue(&playlists_working);
}

void
queue_playlist(sp_playlist *pl, pl_queue *playlist, int end) {
    /* and a partridge in a pear tree */
    struct pl_queue_entry *t = (struct pl_queue_entry *)malloc(sizeof(struct pl_queue_entry));
    t->pl = pl;
    if (end == HEAD) {
        STAILQ_INSERT_HEAD(playlist, t, entries);
    } else {
        STAILQ_INSERT_TAIL(playlist, t, entries);
    }
}

void
queue_pending(sp_playlist *pl) {
    queue_playlist(pl, &playlists_pending, TAIL);
}

void
queue_pending_first(sp_playlist *pl) {
    queue_playlist(pl, &playlists_pending, HEAD);
}

void
queue_working(sp_playlist *pl) {
    queue_playlist(pl, &playlists_working, TAIL);
}

sp_playlist *
dequeue_playlist(pl_queue *playlist) {
    sp_playlist *r_pl;
    struct pl_queue_entry *t;

    /* empty list returns NULL on dequeue */
    if (STAILQ_EMPTY(playlist)) {
       return NULL;
    }

    t = STAILQ_FIRST(playlist);
    r_pl = t->pl; /* save our playlist pointer */

    /* no need for the queue entry any more */
    STAILQ_REMOVE_HEAD(playlist, entries);
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

    STAILQ_FOREACH(np, &playlists_working, entries) {
        if (np->pl == pl) {
            fprintf(stderr, "W-  %s\n", sp_playlist_name(np->pl));
            STAILQ_REMOVE(&playlists_working, np, pl_queue_entry, entries);
        } else {
            fprintf(stderr, "W=  %s\n", sp_playlist_name(np->pl));
        }
    }
}

int
still_working(void) {
    return ! STAILQ_EMPTY(&playlists_working);
}

int
still_pending(void) {
    return ! STAILQ_EMPTY(&playlists_pending);
}

int
deinit_finished_working(int(*seek)(sp_playlist*),void(*destroy)(sp_playlist*)) {
    struct pl_queue_entry *np;
    int rv = 0;

    STAILQ_FOREACH(np, &playlists_working, entries) {
        if ((*seek)(np->pl)) { // remove from working
            fprintf(stderr, "W!  %s\n", sp_playlist_name(np->pl));
            (*destroy)(np->pl);
            rv = 1; /* we've removed a playlist => free slot */
        } else {
            fprintf(stderr, "W?  %s\n", sp_playlist_name(np->pl));
        }
    }

    return rv;
}

void
print_working(char *prefix)
{
    struct pl_queue_entry *np;
    int i=0;

    if (STAILQ_EMPTY(&playlists_working)) {
        fprintf(stderr, "Q. %s EMPTY\n", prefix);
        return;
    }

    STAILQ_FOREACH(np, &playlists_working, entries) {
        fprintf(stderr, "Q. %s %d %p %s\n", prefix, i, np->pl,
                np->pl ? sp_playlist_name(np->pl) : "[NULL]");
        i++;
    }
}

void
print_pending(char *prefix)
{
    struct pl_queue_entry *np;
    int i=0;

    if (STAILQ_EMPTY(&playlists_pending)) {
        fprintf(stderr, "Q. %s EMPTY\n", prefix);
        return;
    }

    STAILQ_FOREACH(np, &playlists_pending, entries) {
        fprintf(stderr, "Q. %s %d %p %s\n", prefix, i, np->pl,
                np->pl ? sp_playlist_name(np->pl) : "[NULL]");
        i++;
    }
}
