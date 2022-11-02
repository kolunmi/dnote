#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/Xlib.h>

#include "common.h"
#include "util.h"

void
usage(void) {
    fputs("usage: dnote [OPTS]\n"
	  "	-id [string]		asscociate message with id\n"
	  "	-exp [seconds]		time until expiration\n"
	  "	-minw [pixels]		minimum window width\n"
	  "	-c			center text\n"
	  "	-nc			don't center text\n"
	  "	-loc [0-8]		window location option\n"
	  "	-pbar [val] [out of]	construct a progress bar\n"
	  "	-cmd [command]		run shell command when window is clicked\n"
	  "	-img [filepath]		render png to window\n"
	  "	-img-header		position png at top of window\n"
	  "	-img-inline		position png next to text\n"
	  "	-v			print version info\n"
	  "	-h			print this help text\n"
	  , stderr);
    exit(1);
}


int
main(int argc, char *argv[]) {
    int i, j;
    char emit[MESSAGE_SIZE];
    char buf[BUFSIZ];
    size_t len, tmplen;
    unsigned int tmp1, tmp2;
    float tmpf;

    int sock_fd;
    struct sockaddr_un sock_address;
    char dpy_name[64], *dpy_name_ptr;
    DIR *dir;
    struct dirent *de;
    
    len = 0;
    
    for (i = 1; i < argc; i++) {
	/* these options take no arguments */
	if (!strcmp(argv[i], "-v")) {
	    puts("dnote-"VERSION);
	    exit(0);
	}
	else if (!strcmp(argv[i], "-c")) {
	    snprintf(buf, sizeof buf, "%c", OPTION_JUSTIFY_CENTER);
	}
	else if (!strcmp(argv[i], "-nc")) {
	    snprintf(buf, sizeof buf, "%c", OPTION_JUSTIFY_LEFT);
	}
	else if (!strcmp(argv[i], "-img-header")) {
	    snprintf(buf, sizeof buf, "%c", OPTION_HEADER_IMAGE);
	}
	else if (!strcmp(argv[i], "-img-inline")) {
	    snprintf(buf, sizeof buf, "%c", OPTION_INLINE_IMAGE);
	}
	else if (i + 1 == argc) {
	    usage();
	}
	/* these options take 1 argument */
	else if (!strcmp(argv[i], "-minw")) {
	    tmp1 = atoi(argv[++i]);
	    snprintf(buf, sizeof buf, "%c%i", OPTION_MIN_WIDTH, tmp1);
	}
	else if (!strcmp(argv[i], "-exp")) {
	    tmpf = atof(argv[++i]);
	    if (tmpf)
		snprintf(buf, sizeof buf, "%c%f", OPTION_EXPIRE, tmpf);
	    else
		snprintf(buf, sizeof buf, "%c", OPTION_NO_EXPIRE);
	}
	else if (!strcmp(argv[i], "-loc")) {
	    tmp1 = atoi(argv[++i]);
	    if (tmp1 > 8)
		die("-loc : invalid location specifier");
	    snprintf(buf, sizeof buf, "%c%i", OPTION_LOCATION, tmp1);
	}
	else if (!strcmp(argv[i], "-id")) {
	    i++;
	    tmp1 = strlen(argv[i]);
	    if (tmp1 > MAX_ID_LEN) {
		argv[i][MAX_ID_LEN - 1] = '\0';
		tmp1 = MAX_ID_LEN;
	    }
	    snprintf(buf, sizeof buf, "%c%s", OPTION_ID, argv[i]);
	}
	else if (!strcmp(argv[i], "-cmd")) {
	    snprintf(buf, MAX_SHCMD_LEN, "%c%s", OPTION_SHELL_COMMAND, argv[++i]);
	}
	else if (!strcmp(argv[i], "-img")) {
	    buf[0] = OPTION_IMAGE_PATH;
	    if (realpath(argv[++i], buf + 1) == NULL)
		die("-img : could not find file");
	}
	else if (i + 2 >= argc) {
	    usage();
	}
	/* these options take 2 arguments */
	else if (!strcmp(argv[i], "-pbar")) {
	    tmp1 = atoi(argv[++i]);
	    tmp2 = atoi(argv[++i]);
	    if (!tmp2 || tmp1 > tmp2)
		die("-pbar : invalid arguments");
	    snprintf(buf, sizeof buf, "%c%i/%i", OPTION_PROGRESS_BAR, tmp1, tmp2); 
	}
	else
	    usage();

	tmplen = len + strlen(buf) + 1;
	if (tmplen >= MESSAGE_SIZE)
	    die("message args exceed max size");
	strcpy(emit + len, buf);
	len = tmplen;
    }


    if (len < MESSAGE_SIZE - 1)
	emit[len++] = '\n';
    else
	die("message args exceed max size");

    
    sock_address.sun_family = AF_UNIX;
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	die("could not create the socket");
    
    dpy_name_ptr = XDisplayName(NULL);
    if (dpy_name_ptr[0] != '\0') {
	if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, SOCKET_PATH, dpy_name_ptr) == -1)
	    die("could not write the socket path");
	if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof(sock_address)) == -1)
	    die("could not to connect to the socket");
    }
    else {
	report(0, TITLE_STATUS, "could not find display in environment, searching for socket");
	
	if ((dir = opendir(X_DISPLAY_DIR)) == NULL)
	    die("could not find a socket");

	dpy_name_ptr = NULL;
	
	while ((de = readdir(dir)) != NULL) {
	    if (de->d_name[0] != 'X')
		continue;
	    
	    snprintf(dpy_name, sizeof dpy_name, ":%s", de->d_name + 1);
	    if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, SOCKET_PATH, dpy_name) == -1)
		die("could not write the socket path");
	    if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof(sock_address)) == -1)
		continue;

	    dpy_name_ptr = (char *) &dpy_name;
	    break;
	}

	if (dpy_name_ptr == NULL)
	    die("could not find a socket");

	report(0, TITLE_STATUS, "found socket on display %s", dpy_name_ptr);
    }

    
    for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
	tmplen = len + strlen(buf);
	if (tmplen >= MESSAGE_SIZE) {
	    report(1, TITLE_WARNING, "message exceeds max size, truncating");
	    break;
	}
	strcpy(emit + len, buf);
	len = tmplen;
    }

    
    if (send(sock_fd, emit, len, 0) == -1)
	die("failed to send the data");
}
