# Backup Spotify Playlists

## Requirements

### Application key

You need a
[Spotify Developer](https://developer.spotify.com/technologies/libspotify/)
application key in C format in appkey.c.

### libspotify

Grab the appropriate version of libspotify from [Spofity Developer download](http://developer.spotify.com/technologies/libspotify/#download).

### Ruby 1.9 + xspf gem

If you want to convert the raw output to XSPF files, you'll need the XSPF module.

    gem install xspf

### Others

* __BSD Queue functions__: Core on Linux and OS X.
* __pthreads__: Core on Linux and OS X.

## Building

    redo clean all

If you don't have `redo` installed, the minimal alternative `mdo` is included.

    ./mdo clean all

That should create the `px` binary.

## Running

You definitely want to ignore stderr unless debugging.

    ./px -u [username] -p [password] 2> /dev/null > pl.raw

`pl.raw` is an agnostic dump of the playlist contents.

XSPF output comes from `xspf.rb`

    mkdir -p playlists
    ruby xspf.rb < pl.raw

That will create numbered xspf files in the playlist directory.

## Caveats

Only tracks have URIs.  The XSPF format has no canonical identifier for
albums or artists.  They could be shoved in extension, link, or meta elements
but Ruby xspf (as of 0.4) doesn't handle these correctly.

Sometimes libspotify seems to ignore a single (new?) playlist.  Investigations are ongoing.  Often picked up on the next run.
