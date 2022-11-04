# dnote - notification utility for X based on dmenu
.POSIX:

CCFLAGS  = $(CFLAGS)
CLDFLAGS = -L/usr/X11R6/lib -lX11 $(LDFLAGS)

DCFLAGS  = $(CCFLAGS)
DLDFLAGS = $(CLDFLAGS)

ECFLAGS  = $(CCFLAGS) -I/usr/X11R6/include -DXINERAMA -I/usr/include/freetype2
ELDFLAGS = $(CLDFLAGS) -lXinerama -lfontconfig -lXft -lpthread -lrt -lpng

PREFIX = /usr/local

CC = cc

all: dnote dnotec dnoted

dnote: dnote.c util.c client.c
	$(CC) -o $@ $(CCFLAGS) $(CLDFLAGS) $^

dnotec: dnotec.c util.c client.c
	$(CC) -o $@ $(DCFLAGS) $(DLDFLAGS) $^

dnoted: dnoted.c util.c drw.c image.c
	$(CC) -o $@ $(ECFLAGS) $(ELDFLAGS) $^

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f dnote $(DESTDIR)$(PREFIX)/bin
	cp -f dnotec $(DESTDIR)$(PREFIX)/bin
	cp -f dnoted $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dnote
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dnotec
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dnoted

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dnote \
		$(DESTDIR)$(PREFIX)/bin/dnotec
		$(DESTDIR)$(PREFIX)/bin/dnoted

clean:
	rm -r dnote dnotec dnoted

.PHONY: all clean install uninstall
