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
/// Handle to the playlist currently being played
static sp_playlist *g_jukeboxlist;

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
	if (pl != g_jukeboxlist)
		return;

	printf("jukebox: %d tracks were added\n", num_tracks);
	fflush(stdout);
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
	printf("jukebox: %d tracks were removed\n", num_tracks);
	fflush(stdout);
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
	if (pl != g_jukeboxlist)
		return;

	printf("jukebox: %d tracks were moved around\n", num_tracks);
	fflush(stdout);
}

static int count_playlists_loaded = 0;
static int count_playlists_shown  = 0, cp_original = 0;

/* forward reference */
void kill_cb(sp_playlist *pl);
sp_playlistcontainer *g_pc;
static void notify_main_thread(sp_session *sess);

void show_playlist(sp_playlist *pl)
{
    int nt = sp_playlist_num_tracks(pl);
    int j;

    fprintf(stderr, "%d %s", sp_playlist_num_tracks(pl), sp_playlist_name(pl));
    
    for(j=0; j<nt; j++) {
        sp_track *st = sp_playlist_track(pl, j);

        if (st && sp_track_is_loaded(st)) {
            char track_uri[1024], album_uri[1024];
            {
                sp_link *t_sl = sp_link_create_from_track(st, 0);
                sp_link_as_string(t_sl, track_uri, 1024);
                sp_link_release(t_sl);
            }
            {
                sp_album *sa = sp_track_album(st);
                sp_link *a_sl = sp_link_create_from_album(sa);
                sp_link_as_string(a_sl, album_uri, 1024);
                sp_link_release(a_sl);
            }
            fprintf(stderr, "  #%d %s %s\n", j+1, track_uri, album_uri);
            /* if we've not seen this album before, queue up a metadata search */
            // sp_track_release(st);
        }
    }
    count_playlists_shown--;
    fprintf(stderr, "%d/%d playlists unshown\n", count_playlists_shown, cp_original);
    if (count_playlists_shown == 0) {
        fprintf(stderr, "THEORETICALLY WE'RE FINISHED\n");
        sleep(30);
        sp_session_logout(g_sess);
        exit(0);
    }
    sp_playlist_release(pl);
}

/* forward reference */
static sp_playlist_callbacks pl_callbacks;

static void playlist_metadata(sp_playlist *pl, void *userdata)
{
    int i, nt = sp_playlist_num_tracks(pl);
    int loaded = 0;

    fprintf(stderr, "PM %p\n", pl);

    for(i=0; i<nt; i++) {
        sp_track *st = sp_playlist_track(pl, i);

        if (st && sp_track_error(st) == SP_ERROR_OK) {
            loaded++;
        }
    }
    if (loaded == nt) {
        fprintf(stderr, "FULL %s\n", sp_playlist_name(pl));
        kill_cb(pl);
        show_playlist(pl);

        // take next playlist off queue and add callbacks
        {
            sp_playlist *next = dequeue_playlist();
            if (next == NULL) {
                fprintf(stderr, "Empty queue, all playlists fully loaded, exiting.\n");
                sleep(5);
                sp_session_logout(g_sess);
                exit(0);
            }
            fprintf(stderr, "Dequeued [%s] for fetching\n", sp_playlist_name(next));
            sp_playlist_add_callbacks(next, &pl_callbacks, (void*)0x1);
        }

    } else {
        fprintf(stderr, "%d/%d %s\n", loaded, nt, sp_playlist_name(pl));
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
    if (userdata != 0) {
        sp_link *spl = sp_link_create_from_playlist(pl);
        if (spl) { /* successful link creation = loaded the playlist */
            int pi;

            sp_link_release(spl);
            fprintf(stderr, "+P u=%p %s (%d) %d\n", userdata, sp_playlist_name(pl), sp_playlist_num_tracks(pl), count_playlists_loaded);

            sp_playlist_add_ref(pl);
            kill_cb(pl);

            // add playlist to end of queue without callbacks
            fprintf(stderr, "metadata callback #%d [%s] to the queue\n", pi, sp_playlist_name(pl));
            // when the queue is N long, process the head of the queue
            sp_playlist_add_callbacks(pl, &md_callbacks, (void*)0x2);

            {
                sp_track *st;
                int k;

                for(k=0; k<sp_playlist_num_tracks(pl); k++) {
                    st = sp_playlist_track(pl, k);
                    sp_track_add_ref(st);
                }
            }

            count_playlists_loaded--;
            if (count_playlists_loaded == 0) {
               fprintf(stderr, "ALL PLAYLISTS LOADED!\n");
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
    int indent = 0;

	fprintf(stderr, "jukebox: Rootlist synchronized (%d playlists)\n",
	    sp_playlistcontainer_num_playlists(pc));
    count_playlists_loaded = sp_playlistcontainer_num_playlists(pc);

    /* now we can write them all out to xspf */
	for (i = 0; i < count_playlists_loaded; ++i) {
		sp_playlist *pl = sp_playlistcontainer_playlist(pc, i);
        sp_playlist_type t = sp_playlistcontainer_playlist_type(pc, i);

        if (t == SP_PLAYLIST_TYPE_PLAYLIST) {
            fprintf(stderr, "Storing #%d [%s]\n", i, sp_playlist_name(pl));
            queue_playlist(pl);
            stored++;
        } else {
            fprintf(stderr, "Ignoring %d because empty or folder\n", i);
        }
    }
    fprintf(stderr, "stored=%d\n", stored);

    /* fire off the first N playlists to fetch - currently 1 */
    for(i=0; i<1; i++) {
        sp_playlist *first = dequeue_playlist();
        sp_playlist_add_callbacks(first, &pl_callbacks, 0x1);
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
    init_playlist_queue();

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
