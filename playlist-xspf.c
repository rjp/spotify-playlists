/**
 * Copyright (c) 2006-2010 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * This example application shows parts of the playlist and player submodules.
 * It also shows another way of doing synchronization between callbacks and
 * the main thread.
 *
 * This file is part of the libspotify examples suite.
 */

#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <libspotify/api.h>

#include "pl-queue.h"
#define SPE(e) if(e){fprintf(stderr, "! %s:%d %s\n", __FILE__, __LINE__, sp_error_message(e));};

/* --- Data --- */
/// The application key is specific to each project, and allows Spotify
/// to produce statistics on how our service is used.
extern const uint8_t g_appkey[];
/// The size of the application key.
extern const size_t g_appkey_size;

/// Synchronization mutex for the main thread
static pthread_mutex_t g_notify_mutex;
/// Synchronization condition variable for the main thread
static pthread_cond_t g_notify_cond;
/// Synchronization variable telling the main thread to process events
static int g_notify_do;
/// The global session handle
static sp_session *g_sess;

static pthread_mutex_t g_working_mutex;
static pthread_t g_working_scanner;

// global error variable
sp_error e;

/* --------------------------  PLAYLIST CALLBACKS  ------------------------- */
/**
 * Callback from libspotify, saying that a track has been added to a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track handles
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  position    Where the tracks were inserted
 * @param  userdata    The opaque pointer
 */
static void tracks_added(sp_playlist *pl, sp_track * const *tracks,
                         int num_tracks, int position, void *userdata)
{
	fprintf(stderr, "[%s]: %d tracks were added\n", sp_playlist_name(pl), num_tracks);
}

/**
 * Callback from libspotify, saying that a track has been added to a playlist.
 *
 * @param  pl          The playlist handle
 * @param  tracks      An array of track indices
 * @param  num_tracks  The number of tracks in the \c tracks array
 * @param  userdata    The opaque pointer
 */
static void tracks_removed(sp_playlist *pl, const int *tracks,
                           int num_tracks, void *userdata)
{
	fprintf(stderr, "[%s]: %d tracks were removed\n", sp_playlist_name(pl), num_tracks);
}

/**
 * Callback from libspotify, telling when tracks have been moved around in a playlist.
 *
 * @param  pl            The playlist handle
 * @param  tracks        An array of track indices
 * @param  num_tracks    The number of tracks in the \c tracks array
 * @param  new_position  To where the tracks were moved
 * @param  userdata      The opaque pointer
 */
static void tracks_moved(sp_playlist *pl, const int *tracks,
                         int num_tracks, int new_position, void *userdata)
{
	fprintf(stderr, "[%s]: %d tracks were shuffled\n", sp_playlist_name(pl), num_tracks);
}

static int count_playlists_loaded = 0;
static int count_playlists_shown  = 0;

/* forward reference */
void kill_cb(sp_playlist *pl);
void kill_md(sp_playlist *pl);
sp_playlistcontainer *g_pc;
static void notify_main_thread(sp_session *sess);

int show_playlist(sp_playlist *pl)
{
    int nt = sp_playlist_num_tracks(pl);
    int j;
    sp_link *pl_link = sp_link_create_from_playlist(pl);
    char playlist_uri[1024];

    if (pl_link == NULL) {
        fprintf(stderr, "pl_link is NULL, something has gone wrong.\n");
        return 0;
    }

    sp_link_as_string(pl_link, playlist_uri, 1024);
    sp_link_release(pl_link);
    sp_user *pl_user = sp_playlist_owner(pl);

    if (!pl_user) {
        fprintf(stderr, "There is no owner of this playlist?\n");
        exit(1);
    }

    printf("PLAYLIST %p %d %s\n", pl, sp_playlist_num_tracks(pl), sp_playlist_name(pl));
    printf("OWNER %p %s\n", pl, sp_user_canonical_name(pl_user));

    {
        const char *desc = sp_playlist_get_description(pl);
        if (desc) {
            printf("DESCRIPTION %p %s\n", pl, desc);
        }
    }
    
    for(j=0; j<nt; j++) {
        sp_track *st = sp_playlist_track(pl, j);
        int na = sp_track_num_artists(st);

        if (st && sp_track_is_loaded(st)) {
            {
                sp_user *user = sp_playlist_track_creator(pl, j);
                if (!user) {
                    printf("TRACK:CREATOR %p %d %s\n", pl, j, sp_user_canonical_name(pl_user));
                } else {
                    printf("TRACK:CREATOR %p %d %s\n", pl, j, sp_user_canonical_name(user));
                }
            }
            {
                char track_uri[1024];
                sp_link *t_sl = sp_link_create_from_track(st, 0);
                sp_link_as_string(t_sl, track_uri, 1024);
                sp_link_release(t_sl);
                printf("TRACK:URI %p %d %s\n", pl, j, track_uri);
                printf("TRACK:NAME %p %d %s\n", pl, j, sp_track_name(st));
                printf("TRACK:DURATION %p %d %d\n", pl, j, sp_track_duration(st));
                printf("TRACK:EPOCH %p %d %d\n", pl, j, sp_playlist_track_create_time(pl, j));
            }
            {
                char album_uri[1024];
                sp_album *sa = sp_track_album(st);
                sp_link *a_sl = sp_link_create_from_album(sa);
                sp_link_as_string(a_sl, album_uri, 1024);
                sp_link_release(a_sl);
                printf("ALBUM:URI %p %d %s\n", pl, j, album_uri);
                printf("ALBUM:NAME %p %d %s\n", pl, j, sp_album_name(sa));
            }
            {
                int i;
                for(i=0; i<na; i++) {
                    char artist_uri[1024];
                    sp_artist *artist = sp_track_artist(st, i);
                    sp_link *l_artist = sp_link_create_from_artist(artist);
                    sp_link_as_string(l_artist, artist_uri, 1024);
                    sp_link_release(l_artist);
                    printf("ARTIST:URI %p %d %d %s\n", pl, j, i, artist_uri);
                    printf("ARTIST:NAME %p %d %d %s\n", pl, j, i, sp_artist_name(artist));
                }
            }
            printf("TRACK:END %p %d\n", pl, j);
        }
    }
    printf("PLAYLIST:END %p\n", pl);
    count_playlists_shown++;
    fprintf(stderr, "%d playlists shown\n", count_playlists_shown);

    return 1;
}

/* forward reference */
static sp_playlist_callbacks pl_callbacks;
static sp_playlist_callbacks md_callbacks;

int
playlist_populated(sp_playlist *pl)
{
    int i, nt = sp_playlist_num_tracks(pl);
    int loaded = 0;

    for(i=0; i<nt; i++) {
        sp_track *st = sp_playlist_track(pl, i);

        if (st && sp_track_error(st) == SP_ERROR_OK) {
            loaded++;
        } else {
            const char *x = st ? sp_track_name(st) : "[NULL]";
            fprintf(stderr, "%%! %d/%d %s\n", i, nt, x);
        }
    }
    fprintf(stderr, "%% %d/%d %s\n", loaded, nt, sp_playlist_name(pl));

    return nt == loaded;
}

void
playlist_deinit(sp_playlist *pl) {
    if (show_playlist(pl)) {
        fprintf(stderr, "FULL %s\n", sp_playlist_name(pl));
        kill_cb(pl);
        kill_md(pl);
        remove_working(pl);
        sp_playlist_release(pl);
    } else {
        fprintf(stderr, "ERROR in show, leaving on pending list\n");
    }
}

void
finished_working(void)
{
    fprintf(stderr, "All queues empty, exiting\n");
    sleep(5);
    sp_session_logout(g_sess);
    exit(0);
}

void
playlist_next(void)
{
    sp_playlist *next = NULL;
    do {
        fprintf(stderr, "Trying to fetch the next playlist\n");
        next = dequeue_pending();

        if (next == NULL) {
            if (still_working()) {
                fprintf(stderr, "Empty pending queue, still processing\n");
                return;
            } else {
                finished_working();
            }
        }

        if (playlist_populated(next)) {
            fprintf(stderr, "Dequeue-skip [%s]\n", sp_playlist_name(next));
            playlist_deinit(next);
            next = NULL;
        } else {
            fprintf(stderr, "Dequeue-fetch [%s]\n", sp_playlist_name(next));
            e = sp_playlist_add_callbacks(next, &pl_callbacks, (void*)0x1);
            SPE(e);
            queue_working(next);
        }
    } while (next == NULL);
}

static void playlist_metadata(sp_playlist *pl, void *userdata)
{
    if (playlist_populated(pl)) {
        playlist_deinit(pl);
        playlist_next();
    } else {
        fprintf(stderr, "Loading: %s\n", sp_playlist_name(pl));
    }
}

struct xx {
    sp_playlist *pl;
    int tagged;
    int userdata;
} playlists[1024];
int stored = 0;

static sp_playlist_callbacks md_callbacks = {
    .playlist_metadata_updated = &playlist_metadata,
};

static void playlist_state_changed(sp_playlist *pl, void *userdata)
{
    fprintf(stderr, "PSC %p %s\n", userdata, sp_playlist_name(pl));
    if (userdata != 0) {
        sp_link *spl = sp_link_create_from_playlist(pl);
        fprintf(stderr, "PSC/L %p\n", spl);
        if (spl) { /* successful link creation = loaded the playlist */
            sp_link_release(spl);
            fprintf(stderr, "+P u=%p %s (%d) %d\n", userdata, sp_playlist_name(pl), sp_playlist_num_tracks(pl), count_playlists_loaded);

            sp_playlist_add_ref(pl);
            kill_cb(pl);

            // add playlist to end of queue without callbacks
            fprintf(stderr, "metadata callback [%s] to the queue\n", sp_playlist_name(pl));
            // when the queue is N long, process the head of the queue
            sp_playlist_add_callbacks(pl, &md_callbacks, (void*)0x2);

            {
                int k;

                if(playlist_populated(pl)) {
                    playlist_deinit(pl);
                    playlist_next();
                } else {
                    for(k=0; k<sp_playlist_num_tracks(pl); k++) {
                        sp_track *st = sp_playlist_track(pl, k);
                        fprintf(stderr, "T %d/%p %d %s\n", k, st, sp_track_error(st), sp_playlist_name(pl));
                    }
                }
            }
        }
        else {
            fprintf(stderr, "?P %p\n", pl);
        }
    }
    else {
        fprintf(stderr, "-P %p\n", pl);
    }
}


/**
 * The callbacks we are interested in for individual playlists.
 */
static sp_playlist_callbacks pl_callbacks = {
	.tracks_added = &tracks_added,
	.tracks_removed = &tracks_removed,
	.tracks_moved = &tracks_moved,
    .playlist_state_changed = &playlist_state_changed,
};

void kill_cb(sp_playlist *pl) {
     sp_playlist_remove_callbacks(pl, &pl_callbacks, (void*)0x1);
}

void kill_md(sp_playlist *pl) {
     sp_playlist_remove_callbacks(pl, &md_callbacks, (void*)0x2);
}


/* --------------------  PLAYLIST CONTAINER CALLBACKS  --------------------- */
/**
 * Callback from libspotify, telling us a playlist was added to the playlist container.
 *
 * We add our playlist callbacks to the newly added playlist.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the added playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_added(sp_playlistcontainer *pc, sp_playlist *pl,
                           int position, void *userdata)
{
    int t = sp_playlistcontainer_playlist_type(pc, position);
    fprintf(stderr, "Callbacks: %d %d %p\n", position, t, pl);
}

/**
 * Callback from libspotify, telling us a playlist was removed from the playlist container.
 *
 * This is the place to remove our playlist callbacks.
 *
 * @param  pc            The playlist container handle
 * @param  pl            The playlist handle
 * @param  position      Index of the removed playlist
 * @param  userdata      The opaque pointer
 */
static void playlist_removed(sp_playlistcontainer *pc, sp_playlist *pl,
                             int position, void *userdata)
{
	sp_playlist_remove_callbacks(pl, &pl_callbacks, NULL);
}

/**
 * Callback from libspotify, telling us the rootlist is fully synchronized
 * We just print an informational message
 *
 * @param  pc            The playlist container handle
 * @param  userdata      The opaque pointer
 */
static void container_loaded(sp_playlistcontainer *pc, void *userdata)
{
    int i;

	fprintf(stderr, "jukebox: Rootlist synchronized (%d playlists)\n",
	    sp_playlistcontainer_num_playlists(pc));
    count_playlists_loaded = sp_playlistcontainer_num_playlists(pc);

    /* now we can write them all out to xspf */
	for (i = 0; i < count_playlists_loaded; ++i) {
		sp_playlist *pl = sp_playlistcontainer_playlist(pc, i);
        sp_playlist_type t = sp_playlistcontainer_playlist_type(pc, i);

        if (t == SP_PLAYLIST_TYPE_PLAYLIST) {
            fprintf(stderr, "Storing #%d [%s] %d\n", i, sp_playlist_name(pl), t);
            sp_playlist_add_ref(pl);
            queue_pending(pl);
            stored++;
        } else {
            fprintf(stderr, "Ignoring %d because empty or folder\n", i);
        }
    }
    fprintf(stderr, "stored=%d\n", stored);

    /* fire off the first N playlists to fetch - currently 1 */
    for(i=0; i<10; i++) {
        sp_playlist *first = dequeue_pending();
        e = sp_playlist_add_callbacks(first, &pl_callbacks, (void*)0x1);
        SPE(e);
        queue_working(first);
    }
}

/**
 * The playlist container callbacks
 */
static sp_playlistcontainer_callbacks pc_callbacks = {
	.playlist_added = &playlist_added,
	.playlist_removed = &playlist_removed,
	.container_loaded = &container_loaded,
};


/* ---------------------------  SESSION CALLBACKS  ------------------------- */
/**
 * This callback is called when an attempt to login has succeeded or failed.
 *
 * @sa sp_session_callbacks#logged_in
 */
static void logged_in(sp_session *sess, sp_error error)
{
	sp_playlistcontainer *pc = sp_session_playlistcontainer(sess);

	if (SP_ERROR_OK != error) {
		fprintf(stderr, "jukebox: Login failed: %s\n",
			sp_error_message(error));
		exit(2);
	}

    /* initialise our SLIST */
    init_playlist_queues();

	sp_playlistcontainer_add_callbacks(
		pc,
		&pc_callbacks,
		NULL);

    g_pc = pc;
    sp_playlistcontainer_add_ref(pc); /* stop this disappearing */

	fprintf(stderr, "jukebox: Looking at %d playlists\n", sp_playlistcontainer_num_playlists(pc));
}

/**
 * This callback is called from an internal libspotify thread to ask us to
 * reiterate the main loop.
 *
 * We notify the main thread using a condition variable and a protected variable.
 *
 * @sa sp_session_callbacks#notify_main_thread
 */
static void notify_main_thread(sp_session *sess)
{
	pthread_mutex_lock(&g_notify_mutex);
	g_notify_do = 1;
	pthread_cond_signal(&g_notify_cond);
	pthread_mutex_unlock(&g_notify_mutex);
}

/**
 * The session callbacks
 */
static sp_session_callbacks session_callbacks = {
	.logged_in = &logged_in,
	.notify_main_thread = &notify_main_thread,
	.log_message = NULL,
};

/**
 * The session configuration. Note that application_key_size is an external, so
 * we set it in main() instead.
 */
static sp_session_config spconfig = {
	.api_version = SPOTIFY_API_VERSION,
	.cache_location = "tmp",
	.settings_location = "tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // Set in main()
	.user_agent = "spotify-jukebox-example",
	.callbacks = &session_callbacks,
	NULL,
};
/* -------------------------  END SESSION CALLBACKS  ----------------------- */


/**
 * Show usage information
 *
 * @param  progname  The program name
 */
static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s -u <username> -p <password> -l <listname> [-d]\n", progname);
	fprintf(stderr, "warning: -d will delete the tracks played from the list!\n");
}

void *
scan_working(void *junk)
{
    while (1) {
        fprintf(stderr, "QW working queue cleaner running\n");
        pthread_mutex_lock(&g_working_mutex);
        if (deinit_finished_working(playlist_populated, playlist_deinit)) {
            playlist_next();
        }
        pthread_mutex_unlock(&g_working_mutex);
        fprintf(stderr, "QW working queue cleaner sleeping\n");
        sleep(20);

//        fprintf(stderr, "QP pending queue cleaner running\n");
//        playlist_next();

        fprintf(stderr, "Q? p=%d w=%d\n", still_pending(), still_working());
        if (!still_pending() && !still_working()) {
            finished_working();
        } else {
            print_pending("P!");
            print_working("W!");
        }
        sleep(5);
    }
}

int main(int argc, char **argv)
{
	sp_session *sp;
	sp_error err;
	int next_timeout = 1000;
	const char *username = NULL;
	const char *password = NULL;
	int opt;

	while ((opt = getopt(argc, argv, "u:p:l:d")) != EOF) {
		switch (opt) {
		case 'u':
			username = optarg;
			break;

		case 'p':
			password = optarg;
			break;

		default:
			exit(1);
		}
	}

	if (!username || !password) {
		usage(basename(argv[0]));
		exit(1);
	}

	/* Create session */
	spconfig.application_key_size = g_appkey_size;
    spconfig.initially_unload_playlists = 0;

	err = sp_session_create(&spconfig, &sp);

	if (SP_ERROR_OK != err) {
		fprintf(stderr, "Unable to create session: %s\n",
			sp_error_message(err));
		exit(1);
	}

	g_sess = sp;

	pthread_mutex_init(&g_notify_mutex, NULL);
    pthread_cond_init(&g_notify_cond, NULL);

    pthread_mutex_init(&g_working_mutex, NULL);
    pthread_create(&g_working_scanner, NULL, scan_working, NULL);

	sp_session_login(sp, username, password, 0, NULL);
	pthread_mutex_lock(&g_notify_mutex);

	for (;;) {
		if (next_timeout == 0) {
            fprintf(stderr, "waiting for g_notify_do != 0\n");
            fflush(stderr);
			while(!g_notify_do)
				pthread_cond_wait(&g_notify_cond, &g_notify_mutex);
		} else {
			struct timespec ts;

#if _POSIX_TIMERS > 0
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
            next_timeout = 2.0 * next_timeout;
			ts.tv_sec += next_timeout / 1000;
			ts.tv_nsec += (next_timeout % 1000) * 1000000;

			pthread_cond_timedwait(&g_notify_cond, &g_notify_mutex, &ts);
		}

		g_notify_do = 0;
		pthread_mutex_unlock(&g_notify_mutex);

		do {
			sp_session_process_events(sp, &next_timeout);
		} while (next_timeout == 0);

		pthread_mutex_lock(&g_notify_mutex);
	}

	return 0;
}
