#include <ctype.h>
#include <locale.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "common.h"
#include "drw.h"
#include "util.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
			       && MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { SchemeNorm, SchemeSel, SchemeBar, SchemeLast }; /* color schemes */


void *countdown(void *arg);
void drawcontents(void);
void readmessage(char *msg, int msg_len);
void setdefaults(void);
void setup(void);
void updategeometry(void);
void usage(void);
void *xrun(void *vargp);

char socketpath[BUFSIZ];

static char **lines = NULL;
static unsigned int max_lines, linecnt;

static int bh, mw, mh, wx, wy;
static int lmw, lmh, lwx, lwy;
static unsigned int monw;
static unsigned int monh;
static int lrpad; /* sum of left and right padding */
static int mon = -1, screen;
#ifdef XINERAMA
static int xin_x;
static int xin_y;
#endif

static Display *dpy;
static Window root, parentwin, win;
static Drw *drw;
static Clr *scheme[SchemeLast];

static unsigned int expire;
static unsigned int min_width;
static int center_text;
static unsigned int location;

static unsigned int progressval;
static unsigned int progressof;


#include "config.h"


void
cleanup(void) {
    unlink(socketpath);
    size_t i;
    for (i = 0; i < SchemeLast; i++)
	free(scheme[i]);
    drw_free(drw);
    XSync(dpy, False);
    XCloseDisplay(dpy);
}


void
drawcontents(void)
{
    unsigned int i;
    int y = 0;

    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_rect(drw, 0, 0, mw, mh, 1, 1);

    /* draw vertical list */
    for (i = 0; i < linecnt; i++) {
	drw_text(drw, center_text ? (mw - TEXTW(lines[i]))/2 : 0, y, mw, bh, lrpad / 2, lines[i], 0);
	y += bh;
    }

    if (progressof) {
	drw_setscheme(drw, scheme[SchemeBar]);
		
	if (bar_outer_pad + bar_inner_pad >= bh / 2
	    || bar_outer_pad + bar_inner_pad >= mw / 2)
	    bar_outer_pad = bar_inner_pad = 0;

	drw_rect(drw, bar_outer_pad, y + bar_outer_pad, mw - 2 * bar_outer_pad, bh - 2 * bar_outer_pad, 1, 1);
	drw_rect(drw, bar_outer_pad + bar_inner_pad, y + bar_outer_pad + bar_inner_pad, progressval * (mw - 2 * (bar_outer_pad + bar_inner_pad)) / progressof, bh - 2 * (bar_outer_pad + bar_inner_pad), 1, 0);
	drw_setscheme(drw, scheme[SchemeNorm]);
    }

    drw_map(drw, win, 0, 0, mw, mh);
}


void
*countdown(void *arg)
{
    sleep(*(int *)arg);
    XUnmapWindow(dpy, win);
    XSync(dpy, False);
    return 0;
}


void
readmessage(char *msg, int msg_len)
{
    size_t i, j, size = BUFSIZ;
    int optblk = 1;
    char optbuf[1024], *p;
    
    linecnt = 0;

    for (i = 0, j = 0; i < msg_len; i++) {
	if (msg[i] == '\0' || msg[i] == '\n') {
	    if (optblk) {
		optblk = 0;
		j++;
		continue;
	    }

	    lines[linecnt] = msg + j;
	    j = i + 1;

	    linecnt++;
	    if (progressof && linecnt >= max_lines - 1)
		break;
	    if (linecnt >= max_lines)
		break;
	    if (msg[i] == '\0' || msg[i+1] == '\0')
		break;

	} else if (optblk) {
	    switch (msg[i]) {
	    case 'c':
		center_text = 1;
		break;
	    case 'n':
		center_text = 0;
		break;
	    case 'p':
		strfindtrans(optbuf, msg, '/', &i);
		progressval = atoi(optbuf);
		strfindtrans(optbuf, msg, ':', &i);
		progressof = atoi(optbuf);
		break;
	    case 'z':
		expire = 0;
		break;
	    case 'e':
		strfindtrans(optbuf, msg, ':', &i);
		expire = atoi(optbuf);
		break;
	    case 'w':
		strfindtrans(optbuf, msg, ':', &i);
		min_width = atoi(optbuf);
		break;
	    case 'l':
		optbuf[0] = msg[++i];
		optbuf[1] = '\0';
		location = atoi(optbuf);
		break;
	    }
			
	    j = i + 1;
	}
    }

    for (i = 0; i < linecnt; i++)
	if ((p = strchr(lines[i], '\n')))
	    *p = '\0';
}


void
setup(void)
{
    int x, y, i, j;
    unsigned int du;
    XSetWindowAttributes swa;
    Window w, dw, *dws;
    XWindowAttributes wa;
    XClassHint ch = { "dnoted", "dnoted" };
#ifdef XINERAMA
    XineramaScreenInfo *info;
    Window pw;
    int a, di, n, area = 0;
#endif
    
    /* init appearance */
    for (j = 0; j < SchemeLast; j++)
	scheme[j] = drw_scm_create(drw, colors[j], 2);

    bh = drw->fonts->h + 2;

    /* menu geometry */
#ifdef XINERAMA
    i = 0;
    if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
	if (detectmon) {
	    XGetInputFocus(dpy, &w, &di);
	    if (mon >= 0 && mon < n)
		i = mon;
	    else if (w != root && w != PointerRoot && w != None) {
		/* find top-level window containing current input focus */
		do {
		    if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
			XFree(dws);
		} while (w != root && w != pw);
		/* find xinerama screen with which the window intersects most */
		if (XGetWindowAttributes(dpy, pw, &wa))
		    for (j = 0; j < n; j++)
			if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
			    area = a;
			    i = j;
			}
	    }
	    /* no focused window is on screen, so use pointer location instead */
	    if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
		for (i = 0; i < n; i++)
		    if (INTERSECT(x, y, 1, 1, info[i]))
			break;
	}

	xin_x = info[i].x_org;
	xin_y = info[i].y_org;

	monw = info[i].width;
	monh = info[i].height;

	XFree(info);
    } else
#endif
    {
	if (!XGetWindowAttributes(dpy, parentwin, &wa))
	    die("could not get embedding window attributes: 0x%lx",
		parentwin);

	monw = wa.width;
	monh = wa.height;
    }

    max_lines = 8 * monh / 10 / bh;

    /* create menu window */
    swa.override_redirect = True;
    swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
    win = XCreateWindow(dpy, parentwin, 0, 0, 1, 1, border_width,
			CopyFromParent, CopyFromParent, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XSetWindowBorder(dpy, win, scheme[SchemeSel][ColBg].pixel);
    XSetClassHint(dpy, win, &ch);
    XSelectInput(dpy, win, ButtonPressMask | FocusChangeMask | SubstructureNotifyMask);
    XMapRaised(dpy, win);
}


void
setdefaults(void)
{
    expire = def_expire;
    min_width = def_min_width;
    center_text = def_center_text;
    location = def_location;
    progressof = 0;
}


void
updategeometry(void)
{
    unsigned int i, inputw, tmpmax;

    inputw = tmpmax = 0;
    
    for (i = 0; i < linecnt; i++) {
	tmpmax = TEXTW(lines[i]);
	if (tmpmax > inputw)
	    inputw = tmpmax;
    }

    lmh = mh;
    lmw = mw;
    lwx = wx;
    lwy = wy;

    mh = (linecnt + ((progressof) ? 1 : 0)) * bh;
    mw = MIN(MAX(inputw, min_width), 8 * monw / 10);

    switch (location) {
    case 1:
	wx = 1 * (monw - mw) / 2;
	wy = 9 * (monh - mh) / 10;
	break;
    case 2:
	wx = 9 * (monw - mw) / 10;
	wy = 9 * (monh - mh) / 10;
	break;
    case 3:
	wx = 9 * (monw - mw) / 10;
	wy = 1 * (monh - mh) / 2;
	break;
    case 4:
	wx = 9 * (monw - mw) / 10;
	wy = 1 * (monh - mh) / 10;
	break;
    case 5:
	wx = 1 * (monw - mw) / 2;
	wy = 1 * (monh - mh) / 10;
	break;
    case 6:
	wx = 1 * (monw - mw) / 10;
	wy = 1 * (monh - mh) / 10;
	break;
    case 7:
	wx = 1 * (monw - mw) / 10;
	wy = 1 * (monh - mh) / 2;
	break;
    case 8:
	wx = 1 * (monw - mw) / 10;
	wy = 9 * (monh - mh) / 10;
	break;
    default:
	wx = 1 * (monw - mw) / 2;
	wy = 1 * (monh - mh) / 2;
	break;
    }

#ifdef XINERAMA
    wx += xin_x;
    wy += xin_y;
#endif

    if (mw != lmw || mh != lmh) {
	drw_resize(drw, mw, mh);
	XMoveResizeWindow(dpy, win, wx, wy, mw, mh);
    } else if (wx != lwx || wy != lwy)
	XMoveResizeWindow(dpy, win, wx, wy, mw, mh);
}


void
usage(void)
{
    fputs("usage: dnoted [OPTS]\n"
	  "	-v		print version info\n", 
	  stderr);
}


void
*xrun(void *vargp)
{
    XEvent ev;

    while (!XNextEvent(dpy, &ev)) {
	if (XFilterEvent(&ev, win))
	    continue;
	switch (ev.type) {
	case DestroyNotify:
	    if (ev.xdestroywindow.window != win)
		break;
	    cleanup();
	    exit(1);
	case Expose:
	    if (ev.xexpose.count == 0)
		drw_map(drw, win, 0, 0, mw, mh);
	    break;
	case ButtonPress:
	    if (ev.xbutton.button == Button1)
		XUnmapWindow(dpy, win);
	    break;
	case VisibilityNotify:
	    if (ev.xvisibility.state != VisibilityUnobscured)
		XRaiseWindow(dpy, win);
	    break;
	}
    }

    return 0;
}


int
main(int argc, char *argv[])
{
    unsigned int i, arg;
    char msg[MESSAGE_SIZE];
    
    XWindowAttributes wa;
    int sock_fd = -1;
    struct sockaddr_un sock_address;
    int cli_fd, msg_len;

    pthread_t xhandler, timer;
    int timer_inited = 0;

    for (i = 1; i < argc; i++)
	if (!strcmp(argv[i], "-v")) {
	    puts("dnoted-"VERSION);
	    exit(0);
	} else {
	    usage();
	    exit(1);
	}

    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
	fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
	die("cannot open display");
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    parentwin = root;
    if (!XGetWindowAttributes(dpy, parentwin, &wa))
	die("could not get embedding window attributes: 0x%lx",
	    parentwin);
    drw = drw_create(dpy, screen, root, wa.width, wa.height);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
	die("no fonts could be loaded.");
    lrpad = drw->fonts->h;

#ifdef __OpenBSD__
    if (pledge("stdio rpath", NULL) == -1)
	die("pledge");
#endif

    snprintf(socketpath, sizeof socketpath, SOCKET_PATH, XDisplayName(NULL));

    sock_address.sun_family = AF_UNIX;
    if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, "%s", socketpath) == -1)
	die("could not write the socket path");
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof sock_address) == 0)
	die("socket is already being hosted");
    unlink(socketpath);
    if (bind(sock_fd, (struct sockaddr *) &sock_address, sizeof sock_address) == -1)
	die("could not bind a name to the socket");
    if (listen(sock_fd, SOMAXCONN) < 0)
	die("could not listen to the socket");
    fcntl(sock_fd, F_SETFD, FD_CLOEXEC | fcntl(sock_fd, F_GETFD));

    setup();
    lines = malloc(MESSAGE_SIZE);
    pthread_create(&xhandler, NULL, xrun, NULL);

    for (;;) {
	setdefaults();

	cli_fd = accept(sock_fd, NULL, 0);
	if (cli_fd == -1) {
	    cleanup();
	    die("socket error");
	}

	msg_len = recv(cli_fd, msg, sizeof msg, 0);
	close(cli_fd);

	if (msg_len > 0) {
	    readmessage(msg, msg_len);
	    if (!linecnt)
		continue;
	} else
	    continue;

	XMapRaised(dpy, win);
	updategeometry();
	drawcontents();

	if (timer_inited)
	    pthread_cancel(timer);
	if (expire) {
	    arg = expire;
	    pthread_create(&timer, NULL, countdown, &arg);
	    timer_inited = 1;
	} 
    }

    return 0;
}
