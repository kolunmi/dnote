#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>

#include "common.h"
#include "drw.h"
#include "util.h"


#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
			       && MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)


enum { SchemeNorm, SchemeSel, SchemeBar, SchemeLast }; /* color schemes */


#include "config.h"


typedef struct {

    char id[MAX_ID_LEN];
    int center_text;
    float expire;
    unsigned int min_width;
    unsigned int location;
    unsigned int progress_val, progress_of;
    char cmd[MAX_SHCMD_LEN];
    
} Profile;

typedef struct {
    
    int active, visible, selected;
    Window win;
    int wx, wy;
    unsigned int mw, mh;
    float elapsed;
    Profile prof;
    
} Notification;


void *count_down(void *arg);
void *monitor_socket(void *arg);
void *monitor_x(void *arg);
void arrange(void);
void cancel_inactive(void);
void cleanup(void);
void configure_x_geom(void);
void create_window(Window *win);
void draw_contents(Notification *n);
void make_geometry(Notification *n);
void read_message(void);
void recieve_message(void);
void run(void);
void set_defaults(void);
void usage(void);


static char socketpath[BUFSIZ];
static int sock_fd, cli_fd;

static Display *dpy;
static Window root, parentwin;
static Drw *drw;
static Clr *scheme[SchemeLast];
static int bh;
static unsigned int monw, monh;
static int lrpad;
static int mon = -1, screen;
#ifdef XINERAMA
static int xin_x;
static int xin_y;
#endif

static sem_t mut_resume, mut_check_socket, mut_check_x;
static int message_recieved, event_recieved, reconfigure;

static char msg[MESSAGE_SIZE];
static unsigned int msg_len;

static Notification notifs[MAX_NOTIFICATIONS];
static Notification *order[MAX_NOTIFICATIONS];

static Profile read_prof;

static char **lines = NULL;
static size_t lines_size;
static unsigned int linecnt;
static unsigned int max_lines;


void *
count_down(void *arg)
{
    unsigned int i;
    struct timespec remaining, request = {0, 1000000000 / 20};

    for (;;) {
	nanosleep(&request, &remaining);

	for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	    if (!notifs[i].active || !notifs[i].visible || !notifs[i].prof.expire)
		continue;
	    
	    notifs[i].elapsed += 0.05;
	    if (notifs[i].elapsed >= notifs[i].prof.expire) {
		notifs[i].visible = 0;
		sem_post(&mut_resume);
	    }
	}

    }
    
    return 0;
}


void *
monitor_socket (void *arg)
{
    for (;;) {
	sem_wait(&mut_check_socket);
	cli_fd = accept(sock_fd, NULL, 0);
	message_recieved = 1;
	sem_post(&mut_resume);
    }
    
    return 0;
}


void *
monitor_x(void *arg)
{
    int post;
    unsigned int i;
    XEvent ev;

    for (;;) {
	post = 0;
	
	sem_wait(&mut_check_x);
	
	while (XNextEvent(dpy, &ev) == 0) {
	    if (ev.type == ConfigureNotify) {
		if (ev.xconfigure.window == root)
		    post = reconfigure = 1;
	    }
	    else
		for (i = 0; i < MAX_NOTIFICATIONS; i++) {
		    if (!notifs[i].active)
			continue;

		    if (ev.type == VisibilityNotify) {
			if (ev.xvisibility.window != notifs[i].win)
			    continue;
			if (ev.xvisibility.state != VisibilityUnobscured)
			    XRaiseWindow(dpy, notifs[i].win);
		    }
		    else if (ev.type == DestroyNotify) {
			if (ev.xdestroywindow.window != notifs[i].win)
			    continue;
			post = 1;
			break;
		    }
		    else if (ev.type == ButtonPress) {
			if (ev.xbutton.window != notifs[i].win)
			    continue;
			if (ev.xbutton.button == Button1)
			    notifs[i].selected = 1;
			else if (ev.xbutton.button != Button3)
			    break;
			post = 1;
			break;
		    }
		
		}
	    
	    if (post) {
		notifs[i].visible = 0;
		event_recieved = 1;
		sem_post(&mut_resume);
		break;
	    }
	}
    }

    return 0;
}


void
arrange(void)
{
    unsigned int i;
    Notification *n;

    int offsets[9];
    offsets[0] = 0;
    for (i = 1; i < 9; i++)
	offsets[i] = border_padding;

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	n = order[i];
	
	if (!n->active || !n->visible)
	    continue;

	switch (n->prof.location) {
	case 0:
	    n->wx = (monw - n->mw) / 2;
	    n->wy = (monh - n->mh) / 2 + offsets[0];
	    offsets[0] += n->mh + inter_padding;
	    break;
	case 1:
	    n->wx = (monw - n->mw) / 2;
	    n->wy = (monh - n->mh) - offsets[1];
	    offsets[1] += n->mh + inter_padding;
	    break;
	case 2:
	    n->wx = (monw - n->mw) - border_padding;
	    n->wy = (monh - n->mh) - offsets[2];
	    offsets[2] += n->mh + inter_padding;
	    break;
	case 3:
	    n->wx = (monw - n->mw) - offsets[3];
	    n->wy = (monh - n->mh) / 2;
	    offsets[3] += n->mw + inter_padding;
	    break;
	case 4:
	    n->wx = (monw - n->mw) - border_padding;
	    n->wy = offsets[4];
	    offsets[4] += n->mh + inter_padding;
	    break;
	case 5:
	    n->wx = (monw - n->mw) / 2;
	    n->wy = offsets[5];
	    offsets[5] += n->mh + inter_padding;
	    break;
	case 6:
	    n->wx = border_padding;
	    n->wy = offsets[6];
	    offsets[6] += n->mh + inter_padding;
	    break;
	case 7:
	    n->wx = offsets[7];
	    n->wy = (monh - n->mh) / 2;
	    offsets[7] += n->mw + inter_padding;
	    break;
	case 8:
	    n->wx = border_padding;
	    n->wy = (monh - n->mh) - offsets[8];
	    offsets[8] += n->mh + inter_padding;
	    break;
	default:
	    die("Invalid location");
	}

#ifdef XINERAMA
	n->wx += xin_x;
	n->wy += xin_y;
#endif

	XMapRaised(dpy, n->win);
	XMoveWindow(dpy, n->win, n->wx, n->wy);
    }
    
    XSync(dpy, False);
}


void
cancel_inactive(void)
{
    unsigned int i;
    
    for (i = 0; i < MAX_NOTIFICATIONS; i++)
	if (notifs[i].active && !notifs[i].visible) {
	    notifs[i].active = 0;
	    
	    notifs[i].elapsed = 0;
	    
	    if (notifs[i].selected && notifs[i].prof.cmd[0] != '\0') {
		if (fork() == 0) {
		    execl("/bin/sh", "sh", "-c", notifs[i].prof.cmd, NULL);
		    exit(EXIT_SUCCESS);
		}
		notifs[i].selected = 0;
	    }
	    
	    XUnmapWindow(dpy, notifs[i].win);
	}
}


void
cleanup(void)
{
    size_t i;
    for (i = 0; i < SchemeLast; i++)
	free(scheme[i]);
    drw_free(drw);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    unlink(socketpath);
}


void
configure_x_geom(void)
{
    int x, y, i, j, tmp;
    unsigned int du;
    Window w, dw, *dws;
    XWindowAttributes wa;
    size_t size_tmp;
    
#ifdef XINERAMA
    XineramaScreenInfo *info;
    Window pw;
    int a, di, n, area = 0;
#endif

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
    }
    else
#endif
    {
	if (!XGetWindowAttributes(dpy, parentwin, &wa))
	    die("could not get embedding window attributes: 0x%lx",
		parentwin);

	monw = wa.width;
	monh = wa.height;
    }

    tmp = monh - contents_padding_vertical * 2;
    if (tmp <= 1)
	max_lines = 1;
    else
	max_lines = 9 * tmp / 10 / bh;

    size_tmp = max_lines * sizeof(char *);
    if (size_tmp >= lines_size) {
	lines_size = size_tmp;
	if (!(lines = realloc(lines, lines_size)))
	    die("cannot realloc %u bytes:", lines_size);
    }
}


void
create_window (Window *win)
{
    XSetWindowAttributes swa;
    XClassHint ch = { "dnoted", "dnoted" };
    
    swa.override_redirect = True;
    swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
    *win = XCreateWindow(dpy, parentwin, 0, 0, 1, 1, border_width,
			CopyFromParent, CopyFromParent, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XSetWindowBorder(dpy, *win, scheme[SchemeSel][ColBg].pixel);
    XSetClassHint(dpy, *win, &ch);
    XSelectInput(dpy, *win, ButtonPressMask | FocusChangeMask | SubstructureNotifyMask);
}


void
draw_contents(Notification *n)
{
    unsigned int i;
    int y;

    XMapWindow(dpy, n->win);

    drw_resize(drw, n->mw, n->mh);
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_rect(drw, 0, 0, n->mw, n->mh, 1, 1);

    y = contents_padding_vertical;

    for (i = 0; i < linecnt; i++) {
	drw_text(drw, n->prof.center_text ? (n->mw - TEXTW(lines[i]))/2 : 0, y, n->mw, bh, lrpad / 2, lines[i], 0);
	y += bh;
    }

    if (n->prof.progress_of) {
	drw_setscheme(drw, scheme[SchemeBar]);
		
	if (bar_outer_padding + bar_inner_padding >= bh / 2
	    || bar_outer_padding + bar_inner_padding >= n->mw / 2)
	    bar_outer_padding = bar_inner_padding = 0;

	drw_rect(drw,
		 bar_outer_padding,
		 y + bar_outer_padding,
		 n->mw - 2 * bar_outer_padding,
		 bh - 2 * bar_outer_padding,
		 1, 1);
	drw_rect(drw, bar_outer_padding + bar_inner_padding,
		 y + bar_outer_padding + bar_inner_padding,
		 n->prof.progress_val * (n->mw - 2 * (bar_outer_padding + bar_inner_padding)) / n->prof.progress_of,
		 bh - 2 * (bar_outer_padding + bar_inner_padding),
		 1, 0);
	
	drw_setscheme(drw, scheme[SchemeNorm]);
    }

    drw_map(drw, n->win, 0, 0, n->mw, n->mh);
}


void
make_geometry(Notification *n)
{
    unsigned int i, inputw, tmpmax;
    
    inputw = tmpmax = 0;
    
    for (i = 0; i < linecnt; i++) {
	tmpmax = TEXTW(lines[i]);
	if (tmpmax > inputw)
	    inputw = tmpmax;
    }

    n->mh = (linecnt + ((n->prof.progress_of) ? 1 : 0)) * bh + contents_padding_vertical * 2;
    n->mw = MIN(MAX(inputw, n->prof.min_width), 8 * monw / 10);

    XResizeWindow(dpy, n->win, n->mw, n->mh);
}


void
read_message(void)
{
    size_t i, j;
    int optblk;
    char optbuf[BUFSIZ], *p;

    optblk = 1;
    linecnt = 0;

    for (i = 0, j = 0; i < msg_len; i++) {
	if (optblk) {
	    switch (msg[i]) {
	    case '\n':
		optblk = 0;
		j = i + 1;
		break;
	    case 'c':
		i++;
		read_prof.center_text = 1;
		break;
	    case 'n':
		i++;
		read_prof.center_text = 0;
		break;
	    case 'z':
		i++;
		read_prof.expire = 0;
		break;
	    case 'p':
		i++;
		if ((p = strchr(msg + i, '/')))
		    *p = '\0';
		read_prof.progress_val = atoi(msg + i);
		i += strlen(msg + i) + 1;
		read_prof.progress_of = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case 's':
		i++;
		strncpy(read_prof.cmd, msg + i, sizeof read_prof.cmd);
		i += strlen(msg + i);
		break;
	    case 'e':
		i++;
		read_prof.expire = atof(msg + i);
		i += strlen(msg + i);
		break;
	    case 'w':
		i++;
		read_prof.min_width = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case 'l':
		i++;
		read_prof.location = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case 'i':
		i++;
		strncpy(read_prof.id, msg + i, sizeof read_prof.id);
		i += strlen(msg + i);
		break;
	    }
	}
	else if (msg[i] == '\0' || msg[i] == '\n') {
	    lines[linecnt++] = msg + j;
	    j = i + 1;
	    
	    if (read_prof.progress_of && linecnt >= max_lines - 1)
		break;
	    if (linecnt >= max_lines)
		break;
	    if (msg[i] == '\0') // || msg[i + 1] == '\0')
		break;
	}
    }

    for (i = 0; i < linecnt; i++)
	if ((p = strchr(lines[i], '\n')))
	    *p = '\0';
}


void
recieve_message(void)
{
    unsigned int i;
    Notification *n, *t1, *t2;
	    
    if (cli_fd == -1) {
	cleanup();
	die("socket error");
    }

    msg_len = recv(cli_fd, msg, sizeof msg, 0);
    close(cli_fd);

    for (;;) {
	if (msg_len > 0) {
	    set_defaults();
	    read_message();
	    if (!linecnt)
		break;
	} else
	    break;

	n = NULL;
	if (read_prof.id[0] != '\0')
	    for (i = 0; i < MAX_NOTIFICATIONS; i++)
		if (notifs[i].active) {
		    if (notifs[i].prof.id[0] != '\0'
			&& !strcmp(notifs[i].prof.id, read_prof.id)) {
			n = &notifs[i];
			n->elapsed = 0;
			n->active = 1;
			break;
		    }
		}
	if (n == NULL)
	    for (i = 0; i < MAX_NOTIFICATIONS; i++)
		if (!notifs[i].active) {
		    n = &notifs[i];
		    break;
		}
	if (n == NULL) {
	    fputs("request rejected: too many notifications\n", stderr);
	    break;
	}

	if (n != order[0]) {
	    t1 = order[0];
	    order[0] = n;
		
	    for (i = 1; i < MAX_NOTIFICATIONS; i++) {
		if (order[i] == n) {
		    order[i] = t1;
		    break;
		}
		t2 = order[i];
		order[i] = t1;
		t1 = t2;
	    }
	}
		
	n->prof = read_prof;
	make_geometry(n);
	draw_contents(n);

	n->active = 1;
	n->visible = 1;
		
	break;
    }
}


void
run(void)
{
    pthread_t socket_handler, x_handler, timer;

    message_recieved = event_recieved = reconfigure = 0;

    pthread_create(&socket_handler, NULL, monitor_socket, NULL);
    sem_post(&mut_check_socket);
    
    pthread_create(&x_handler, NULL, monitor_x, NULL);
    sem_post(&mut_check_x);

    pthread_create(&timer, NULL, count_down, NULL);
    
    for (;;) {
	sem_wait(&mut_resume);

	if (reconfigure)
	    configure_x_geom();
	
	if (message_recieved)
	    recieve_message();

	cancel_inactive();
	arrange();
	
	if (message_recieved) {
	    message_recieved = 0;
	    sem_post(&mut_check_socket);
	}
	
	if (event_recieved) {
	    event_recieved = reconfigure = 0;
	    sem_post(&mut_check_x);
	}
    }
}


void
set_defaults(void)
{
    read_prof.id[0] = '\0';
    read_prof.cmd[0] = '\0';
    read_prof.expire = def_expire;
    read_prof.min_width = def_min_width;
    read_prof.center_text = def_center_text;
    read_prof.location = def_location;
    read_prof.progress_of = 0;
}


void
usage(void)
{
    fputs("usage: dnoted [OPTS]\n"
	  "	-v	print version info\n", 
	  stderr);
}


int
main(int argc, char *argv[])
{
    unsigned int i, arg;
    XWindowAttributes wa;
    struct sockaddr_un sock_address;

    if (argc >= 2) {
	if (!strcmp(argv[1], "-v")) {
	    puts("dnoted-"VERSION);
	    exit(0);
	} else {
	    usage();
	    exit(1);
	}
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
    XSelectInput(dpy, root, StructureNotifyMask);
    
    drw = drw_create(dpy, screen, root, wa.width, wa.height);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
	die("no fonts could be loaded.");
    lrpad = drw->fonts->h;
    bh = drw->fonts->h + text_padding;

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

    for (i = 0; i < SchemeLast; i++)
	scheme[i] = drw_scm_create(drw, colors[i], 2);

    lines_size = 0;
    configure_x_geom();

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	notifs[i].active = notifs[i].visible = notifs[i].selected = 0;
	create_window(&notifs[i].win);
	order[i] = &notifs[i];
    }
    
    sem_init(&mut_resume, 0, 1);
    sem_init(&mut_check_socket, 0, 1);
    sem_init(&mut_check_x, 0, 1);
    
    sem_wait(&mut_resume);
    sem_wait(&mut_check_socket);
    sem_wait(&mut_check_x);
    
    run();

    return 0;
}
