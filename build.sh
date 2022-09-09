#!/bin/sh

INSTALL='dnoted dnote'
DEST='/usr/local/bin'

compile() {
    cc $SOURCE $INCLUDE $LINK -o$NAME ||
	exit 1
}

NAME='dnoted'
SOURCE='dnoted.c drw.c util.c'
INCLUDE='-I/usr/X11R6/include -I/usr/include/freetype2'
LINK='-L/usr/X11R6/lib -lX11 -lXinerama -lfontconfig -lXft -lpthread -lrt'
compile

NAME='dnote'
SOURCE='dnote.c util.c'
INCLUDE=''
LINK='-L/usr/X11R6/lib -lX11'
compile

# COMMANDS
for arg; do
    case "$arg" in
	install)
	    cp -f $INSTALL "$DEST"
	    ;;
	uninstall)
	    for iter in $INSTALL; do
		rm -f "$DEST"/"$iter"
	    done
	    ;;
    esac
done
