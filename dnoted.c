#include <dirent.h>
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


#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X, Y)           (drw_fontset_getwidth((X), (Y)) + lrpad)


enum { SchemeNorm, SchemeSel, SchemeBar, SchemeLast }; /* color schemes */


#include "config.h"


typedef struct {

    char id[MAX_ID_LEN];
    int center_text;
    float expire;
    unsigned int min_width;
    unsigned int location;
    int precise_location;
    int px, py;
    unsigned int progress_val, progress_of;
    char cmd[MAX_SHCMD_LEN];
    int inline_image;
    int immutable;
    
} Profile;

typedef struct {

    PngImage img;
    char path[MAX_PATH_LEN];
    
} PngFile;

typedef struct {
    
    int active;
    int visible;
    int selected;
    int draw;
    Window win;
    Drw *drw;
    int wx, wy;
    unsigned int mw, mh;
    float elapsed;
    Profile prof;
    
} Notification;


PngImage *add_image(char *path, int force_reload);
Notification *find_notification(char *id);
void *count_down(void *arg);
void *monitor_socket(void *arg);
void *monitor_x(void *arg);
void arrange(void);
void cancel_inactive(void);
void cleanup(void);
void configure_x_geom(void);
void create_window(Window *win);
void draw_contents(Notification *n, PngImage *img);
void make_geometry(Notification *n, PngImage *img);
void read_dnote_message(void);
void read_dnotec_message(void);
void recieve_message(void);
void run(void);
void set_defaults(void);
void usage(void);


static char socketpath[MAX_PATH_LEN];
static int sock_fd, cli_fd;

static Display *dpy;
static Window root, parentwin;
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

static char *image_path_request;
static PngFile image_files[MAX_IMAGE_MEM];
static PngFile *image_file_ring[MAX_IMAGE_MEM];


PngImage *
add_image(char *path, int force_reload) {
    unsigned int i, j;
    PngImage *img;
    PngFile *pf;

    img = NULL;
    
    for (i = 0; i < MAX_IMAGE_MEM; i++) {
	if (image_file_ring[i]->path[0] == '\0')
	    break;
	if (!strcmp(image_file_ring[i]->path, path)) {
	    img = &image_file_ring[i]->img;
	    break;
	}
    }

    if (img == NULL)
	i = MAX_IMAGE_MEM - 1;
    
    if (img == NULL || force_reload) {
	if ((img = read_png_to_image(&image_file_ring[i]->img, path)) != NULL) {
	    strncpy(image_file_ring[i]->path, path, sizeof image_file_ring[i]->path);
	    report(0, TITLE_STATUS, "loaded image '%s' into memory", path);
	}
	else {
	    report(1, TITLE_WARNING, "could not read image file '%s'", path);
	    return NULL;
	}
    }

    for (j = 0; j < i; j++) {
	pf = image_file_ring[j];
	image_file_ring[j] = image_file_ring[i];
	image_file_ring[i] = pf;
    }

    return img;
}


Notification *
find_notification(char *id) {
    unsigned int i;

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	if (!notifs[i].active)
	    continue;
	if (notifs[i].prof.id[0] == '\0')
	    continue;
	if (!strcmp(id, notifs[i].prof.id))
	    return &notifs[i];
    }
    
    return NULL;
}


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
monitor_socket(void *arg)
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
		    if (!notifs[i].active || !notifs[i].visible)
			continue;

		    if (ev.type == Expose) {
			if (ev.xexpose.window != notifs[i].win)
			    continue;
			if (ev.xexpose.count != 0)
			    break;
			drw_map(notifs[i].drw, notifs[i].win, 0, 0, notifs[i].mw, notifs[i].mh);
			break;
		    }
		    else if (ev.type == VisibilityNotify) {
			if (ev.xvisibility.window != notifs[i].win)
			    continue;
			if (ev.xvisibility.state == VisibilityUnobscured)
			    break;
			XRaiseWindow(dpy, notifs[i].win);
			XSync(dpy, False);
			break;
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
			if (notifs[i].prof.immutable)
			    break;
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

	if (n->prof.precise_location) {
	    n->wx = n->prof.px;
	    n->wy = n->prof.py;
	}
	else {
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
	}

#ifdef XINERAMA
	n->wx += xin_x;
	n->wy += xin_y;
#endif

	XMoveResizeWindow(dpy, n->win, n->wx, n->wy, n->mw, n->mh);
    }

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	n = order[i];
	if (!n->active || !n->visible)
	    continue;
	XMapRaised(dpy, n->win);
	if (!n->draw)
	    continue;
	n->draw = 0;
	drw_map(n->drw, n->win, 0, 0, n->mw, n->mh);
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
	    notifs[i].elapsed = 0.0;
	    
	    if (notifs[i].selected && notifs[i].prof.cmd[0] != '\0') {
		notifs[i].selected = 0;
		report(0, TITLE_COMMAND, "executing '%s'", notifs[i].prof.cmd);
		if (fork() == 0) {
		    execl("/bin/sh", "sh", "-c", notifs[i].prof.cmd, NULL);
		    exit(EXIT_SUCCESS);
		}
	    }
	    
	    XUnmapWindow(dpy, notifs[i].win);
	}
}


void
cleanup(void)
{
    unsigned int i;
    for (i = 0; i < SchemeLast; i++)
	free(scheme[i]);
    for (i = 0; i < MAX_NOTIFICATIONS; i++)
	drw_free(notifs[i].drw);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    unlink(socketpath);
}


void
configure_x_geom(void)
{
    int tmp;
    size_t size_tmp;
    XWindowAttributes wa;
    
#ifdef XINERAMA
    int n;
    XineramaScreenInfo *info;
    
    if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
	xin_x = info[0].x_org;
	xin_y = info[0].y_org;

	monw = info[0].width;
	monh = info[0].height;

	XFree(info);

	report(0, TITLE_STATUS, "geometry configured: %ix%i+%i+%i", monw, monh, xin_x, xin_y);
    }
    else
#endif
    {
	if (!XGetWindowAttributes(dpy, parentwin, &wa))
	    die("could not get embedding window attributes: 0x%lx", parentwin);

	monw = wa.width;
	monh = wa.height;

	report(0, TITLE_STATUS, "geometry configured: %ix%i+0+0", monw, monh);
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
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | VisibilityChangeMask | SubstructureNotifyMask;
    *win = XCreateWindow(dpy, parentwin, 0, 0, 1, 1, border_width,
			CopyFromParent, CopyFromParent, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XSetWindowBorder(dpy, *win, scheme[SchemeSel][ColBg].pixel);
    XSetClassHint(dpy, *win, &ch);
}


void
draw_contents(Notification *n, PngImage *img)
{
    unsigned int i;
    int x, y, ry;

    drw_setscheme(n->drw, scheme[SchemeNorm]);
    drw_rect(n->drw, 0, 0, n->mw, n->mh, 1, 1);
    y = contents_padding_vertical;

    if (img != NULL) {
	if (n->prof.inline_image) {
	    drw_png(n->drw, img, 0, y);

	    if (linecnt * bh < img->h)
		ry = img->h / 2 - linecnt * bh / 2;
	    else
		ry = 0;
	    
	    for (i = 0; i < linecnt; i++) {
		if (ry < img->h) {
		    x = img->w;
		    if (n->prof.center_text)
			x += (n->mw - img->w - TEXTW(n->drw, lines[i]))/2;
		}
		else
		    x = n->prof.center_text ? (n->mw - TEXTW(n->drw, lines[i]))/2 : 0;
	    
		drw_text(n->drw, x, y + ry, n->mw, bh, lrpad / 2, lines[i], 0);
		ry += bh;
	    }
	    
	    if (ry < img->h)
		ry = img->h;

	    y += ry;
	}
	else {
	    drw_png(n->drw, img, (n->mw - img->w)/2, y);
	    y += img->h;
	}
    }
    
    if (img == NULL || !n->prof.inline_image)
	for (i = 0; i < linecnt; i++) {
	    drw_text(n->drw, n->prof.center_text ? (n->mw - TEXTW(n->drw, lines[i]))/2 : 0,
		     y, n->mw, bh, lrpad / 2, lines[i], 0);
	    y += bh;
	}

    
    if (n->prof.progress_of) {
	drw_setscheme(n->drw, scheme[SchemeBar]);

//	if (bar_outer_padding + bar_inner_padding >= bh / 2
//	    || bar_outer_padding + bar_inner_padding >= n->mw / 2)
//	    bar_outer_padding = bar_inner_padding = 0;
	
	drw_rect(n->drw,
		 bar_outer_padding,
		 y + bar_outer_padding,
		 n->mw - 2 * bar_outer_padding,
		 bh - 2 * bar_outer_padding,
		 1, 1);
	drw_rect(n->drw,
		 bar_outer_padding + bar_inner_padding,
		 y + bar_outer_padding + bar_inner_padding,
		 n->prof.progress_val * (n->mw - 2 * (bar_outer_padding + bar_inner_padding)) / n->prof.progress_of,
		 bh - 2 * (bar_outer_padding + bar_inner_padding),
		 1, 0);
	
	drw_setscheme(n->drw, scheme[SchemeNorm]);
    }
}


void
make_geometry(Notification *n, PngImage *img)
{
    unsigned int i, inputw, tmpmax;
    int y;

    n->mh = contents_padding_vertical * 2;
    if (n->prof.progress_of)
	n->mh += bh;

    inputw = tmpmax = 0;
    
    if (img == NULL || !n->prof.inline_image) {
	for (i = 0; i < linecnt; i++) {
	    tmpmax = TEXTW(n->drw, lines[i]);
	    if (tmpmax > inputw)
		inputw = tmpmax;
	}
	
	n->mh += linecnt * bh;
    }
    else {
	for (y = 0, i = 0; i < linecnt; i++) {
	    tmpmax = TEXTW(n->drw, lines[i]);
	    if (y < img->h)
		tmpmax += img->w;
	    y += bh;

	    if (tmpmax > inputw)
		inputw = tmpmax;
	}

	if (y < img->h)
	    y = img->h;
	n->mh += y;
    }

    if (img == NULL || n->prof.inline_image)
	n->mw = MIN(MAX(inputw, n->prof.min_width), 9 * monw / 10);
    else {
	n->mw = MIN(MAX(MAX(inputw, img->w), n->prof.min_width), 9 * monw / 10);
	n->mh += img->h;
    }
}


void
read_dnote_message(void)
{
    size_t i, j;
    int optblk;
    char *p;

    optblk = 1;
    linecnt = 0;

    set_defaults();
    
    for (i = 1, j = 1; i < msg_len; i++) {
	if (optblk) {
	    switch (msg[i]) {
	    case '\n':
		optblk = 0;
		j = i + 1;
		break;
	    case DNOTE_OPTION_JUSTIFY_CENTER:
		i++;
		read_prof.center_text = 1;
		break;
	    case DNOTE_OPTION_JUSTIFY_LEFT:
		i++;
		read_prof.center_text = 0;
		break;
	    case DNOTE_OPTION_NO_EXPIRE:
		i++;
		read_prof.expire = 0;
		break;
	    case DNOTE_OPTION_INLINE_IMAGE:
		i++;
		read_prof.inline_image = 1;
		break;
	    case DNOTE_OPTION_HEADER_IMAGE:
		i++;
		read_prof.inline_image = 0;
		break;
	    case DNOTE_OPTION_IMMUTABLE:
		i++;
		read_prof.immutable = 1;
		break;
	    case DNOTE_OPTION_PROGRESS_BAR:
		i++;
		if ((p = strchr(msg + i, '/')))
		    *p = '\0';
		read_prof.progress_val = atoi(msg + i);
		i += strlen(msg + i) + 1;
		read_prof.progress_of = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_PRECISE_LOCATION:
		i++;
		read_prof.precise_location = 1;
		if ((p = strchr(msg + i, '/')))
		    *p = '\0';
		read_prof.px = atoi(msg + i);
		i += strlen(msg + i) + 1;
		read_prof.py = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_SHELL_COMMAND:
		i++;
		strncpy(read_prof.cmd, msg + i, sizeof read_prof.cmd);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_EXPIRE:
		i++;
		read_prof.expire = atof(msg + i);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_MIN_WIDTH:
		i++;
		read_prof.min_width = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_LOCATION:
		i++;
		read_prof.location = atoi(msg + i);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_ID:
		i++;
		strncpy(read_prof.id, msg + i, sizeof read_prof.id);
		i += strlen(msg + i);
		break;
	    case DNOTE_OPTION_IMAGE_PATH:
		i++;
		image_path_request = msg + i;
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
	    if (msg[i] == '\0')
		break;
	}
    }

    for (i = 0; i < linecnt; i++)
	if ((p = strchr(lines[i], '\n')))
	    *p = '\0';
}


void
read_dnotec_message(void)
{
    size_t i;
    int j, list;
    Notification *n;
    FILE *rsp;
    
    list = 0;

    for (i = 1; i < msg_len; i++) {
	switch (msg[i]) {
	case DNOTEC_OPTION_LIST:
	    i++;
	    list = 1;
	    break;
	case DNOTEC_OPTION_IMAGE_LIST:
	    i++;
	    list = 2;
	    break;
	case DNOTEC_OPTION_CLEAR:
	    i++;
	    for (j = 0; j < MAX_NOTIFICATIONS; j++)
		if (notifs[j].active)
		    notifs[j].visible = 0;
	    break;
	case DNOTEC_OPTION_KILL:
	    i++;
	    if ((n = find_notification(msg + i)) != NULL)
		n->visible = 0;
	    i += strlen(msg + i);
	    break;
	case DNOTEC_OPTION_RENEW:
	    i++;
	    if ((n = find_notification(msg + i)) != NULL)
		n->elapsed = 0.0;
	    i += strlen(msg + i);
	    break;
	case DNOTEC_OPTION_SELECT:
	    i++;
	    if ((n = find_notification(msg + i)) != NULL) {
		n->visible = 0;
		n->selected = 1;
	    }
	    i += strlen(msg + i);
	    break;
	case DNOTEC_OPTION_IMAGE_LOAD:
	    i++;
	    add_image(msg + i, 1);
	    i += strlen(msg + i);
	    break;
	}
    }

    if (list) {
	if ((rsp = fdopen(cli_fd, "w")) != NULL) {
	    if (list == 1) {
		for (j = 0; j < MAX_NOTIFICATIONS; j++)
		    if (order[j]->active && order[j]->prof.id[0] != '\0')
			fprintf(rsp, "%s\n", order[j]->prof.id);
	    }
	    else {
		for (j = 0; j < MAX_IMAGE_MEM; j++)
		    if (image_file_ring[j]->path[0] != '\0')
			fprintf(rsp, "%s\n", image_file_ring[j]->path);
	    }
	    fclose(rsp);
	}
	else
	    report(1, TITLE_WARNING, "could not relay information to client");
    }
}



void
recieve_message(void)
{
    unsigned int i, j;
    Notification *n;
    PngImage *img;
	    
    if (cli_fd == -1) {
	cleanup();
	die("socket error");
    }

    msg_len = recv(cli_fd, msg, sizeof msg, 0);

    for (;;) {
	if (msg_len > 0) {
	    if (msg[0] == DNOTE_PREFIX) {
		read_dnote_message();
		if (!linecnt)
		    break;
	    }
	    else if (msg[0] == DNOTEC_PREFIX) {
		read_dnotec_message();
		break;
	    }
	} else
	    break;
	
	
	n = NULL;
	if (read_prof.id[0] != '\0')
	    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
		if (!order[i]->active)
		    break;
		if (order[i]->prof.id[0] != '\0'
		    && !strcmp(order[i]->prof.id, read_prof.id)) {
		    n = order[i];
		    n->elapsed = 0;
		    n->active = 1;
		    break;
		}
	    }
	if (n == NULL)
	    for (i = 0; i < MAX_NOTIFICATIONS; i++)
		if (!order[i]->active) {
		    n = order[i];
		    break;
		}
	if (n == NULL) {
	    report(1, TITLE_WARNING, "request rejected: too many notifications");
	    break;
	}

	if (i != 0) {
	    for (j = 0; j < i; j++) {
		n = order[j];
		order[j] = order[i];
		order[i] = n;
	    }
	    n = order[0];
	}
	else if (n->active)
	    n->draw = 1;

	img = NULL;
	if (image_path_request != NULL)
	    img = add_image(image_path_request, 0);

	n->prof = read_prof;
	make_geometry(n, img);
	draw_contents(n, img);

	n->active = n->visible = 1;

	break;
    }

    close(cli_fd);
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
    read_prof.precise_location = 0;
    read_prof.inline_image = 0;
    read_prof.progress_of = 0;
    read_prof.immutable = 0;
    image_path_request = NULL;
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
    DIR *dir;
    struct dirent *de;
    char dpy_name[64], *dpy_name_ptr;
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

    
    if ((dpy = XOpenDisplay(NULL)) == NULL) {
	report(0, TITLE_STATUS, "cannot find display in environment, searching");
	
	if ((dir = opendir(X_DISPLAY_DIR)) == NULL)
	    die("could not open display");
	
	while ((de = readdir(dir)) != NULL) {
	    if (de->d_name[0] != 'X')
                continue;
	    
	    snprintf(dpy_name, sizeof dpy_name, ":%s", de->d_name + 1);
	    if ((dpy = XOpenDisplay(dpy_name)) != NULL) {
		dpy_name_ptr = (char *) &dpy_name;
		break;
	    }
	}

	if (dpy == NULL)
	    die("could not open display");
    }
    else
	dpy_name_ptr = XDisplayName(NULL);

    report(0, TITLE_STATUS, "connected to display %s", dpy_name_ptr);

    sock_address.sun_family = AF_UNIX;
    snprintf(socketpath, sizeof socketpath, SOCKET_PATH, dpy_name_ptr);
    if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, "%s", socketpath) == -1)
	die("could not write the socket path");
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	die("could not create the socket");
    if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof sock_address) == 0)
	die("socket is already being hosted");
    unlink(socketpath);
    if (bind(sock_fd, (struct sockaddr *) &sock_address, sizeof sock_address) == -1)
	die("could not bind a name to the socket");
    if (listen(sock_fd, SOMAXCONN) == -1)
	die("could not listen to the socket");
    fcntl(sock_fd, F_SETFD, FD_CLOEXEC | fcntl(sock_fd, F_GETFD));

    report(0, TITLE_STATUS, "hosting socket on %s", socketpath);
    
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
	report(1, TITLE_WARNING, "no locale support");
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    parentwin = root;
    if (!XGetWindowAttributes(dpy, parentwin, &wa))
	die("could not get embedding window attributes: 0x%lx", parentwin);
    XSelectInput(dpy, root, StructureNotifyMask);

    notifs[0].drw = drw_create(dpy, screen, root, wa.width, wa.height);
    if (!drw_fontset_create(notifs[0].drw, fonts, LENGTH(fonts)))
	die("no fonts could be loaded");
    for (i = 0; i < SchemeLast; i++)
	scheme[i] = drw_scm_create(notifs[0].drw, colors[i], 2);
    
    lrpad = notifs[0].drw->fonts->h;
    bh = notifs[0].drw->fonts->h + text_padding;
    
    for (i = 1; i < MAX_NOTIFICATIONS; i++) {
	notifs[i].drw = drw_create(dpy, screen, root, wa.width, wa.height);
	notifs[i].drw->fonts = notifs[0].drw->fonts;
    }

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
	create_window(&notifs[i].win);
	notifs[i].active = notifs[i].visible = notifs[i].selected = notifs[i].draw = 0;
	order[i] = &notifs[i];
    }

    for (i = 0; i < MAX_IMAGE_MEM; i++) {
	image_files[i].img.rows = NULL;
	image_files[i].img.w = image_files[i].img.h = 0;
	image_files[i].path[0] = '\0';
	image_file_ring[i] = &image_files[i];
    }
    
    lines_size = 0;
    configure_x_geom();

    sem_init(&mut_resume, 0, 1);
    sem_init(&mut_check_socket, 0, 1);
    sem_init(&mut_check_x, 0, 1);
    
    sem_wait(&mut_resume);
    sem_wait(&mut_check_socket);
    sem_wait(&mut_check_x);
    
    run();

    return 0;
}
