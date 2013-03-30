# Backup Spotify Playlists

## Requirements

### Application key

You need a
[Spotify Developer](https://developer.spotify.com/technologies/libspotify/)
application key in C format in appkey.c.

### BSD queue functions

Present on Linux and OS X.

### Ruby 1.9 + xspf gem

    gem install xspf

## Building

    redo clean px

If you don't have `redo` installed, the minimal alternative `mdo` is included.

    ./mdo clean px

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

