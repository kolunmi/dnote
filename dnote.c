#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <X11/Xlib.h>

#include "common.h"
#include "util.h"

void
usage(void) {
    fputs("usage: dnote [OPTS]\n"
	  "	-exp [seconds]		time until expiration\n"
	  "	-minw [pixels]		minimum window width\n"
	  "	-c			center text\n"
	  "	-nc			don't center text\n"
	  "	-loc [0-8]		window location option\n"
	  "	-pbar [val] [out of]	construct a progress bar\n"
	  "	-v			print version info\n"
	  "	-h			print this help text\n"
	  , stderr);
    exit(1);
}

int
main(int argc, char *argv[]) {
    int i;
    char *emit;
    char buf[BUFSIZ];
    size_t len = 0;
    unsigned int tmp1, tmp2;

    emit = malloc(MESSAGE_SIZE);

    for (i = 1; i <= argc; i++) {
	if (i >= MESSAGE_SIZE / sizeof *emit)
	    die("message arguments exceed max size");

	if (i == argc) {
	    strcpy(emit + len, "\n");
	    len++;
	    break;
	}

	/* these options take no arguments */
	if (!strcmp(argv[i], "-v")) {
	    puts("dnote-"VERSION);
	    exit(0);
	} else if (!strcmp(argv[i], "-c"))
	    strcpy(buf, "c");
	else if (!strcmp(argv[i], "-nc"))
	    strcpy(buf, "n");
	else if (i + 1 == argc)
	    usage();
	/* these options take 1 argument */
	else if (!strcmp(argv[i], "-minw")) {
	    tmp1 = atoi(argv[++i]);
	    snprintf(buf, sizeof buf, "w%i:", tmp1);
	} else if (!strcmp(argv[i], "-exp")) {
	    tmp1 = atoi(argv[++i]);
	    if (tmp1)
		snprintf(buf, sizeof buf, "e%i:", tmp1);
	    else
		strcpy(buf, "z");
	} else if (!strcmp(argv[i], "-loc")) {
	    tmp1 = atoi(argv[++i]);
	    if (tmp1 > 8)
		die("-loc : invalid location specifier");
	    snprintf(buf, sizeof buf, "l%i", tmp1);
	} else if (i + 2 >= argc)
	    usage();
	/* these options take 2 arguments */
	else if (!strcmp(argv[i], "-pbar")) {
	    tmp1 = atoi(argv[++i]);
	    tmp2 = atoi(argv[++i]);
	    if (!tmp2 || tmp1 > tmp2)
		die("-pbar : invalid arguments");
	    snprintf(buf, sizeof buf, "p%i/%i:", tmp1, tmp2); 
	} else
	    usage();

	strcpy(emit + len, buf);
	len += strlen(buf);
    }

    int sock_fd;
    struct sockaddr_un sock_address;

    sock_address.sun_family = AF_UNIX;

    char *dispname = XDisplayName(NULL);
    if (dispname)
	if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, SOCKET_PATH, dispname) == -1)
	    die("cannot write the socket path");
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	die("failed to create the socket");
    if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof(sock_address)) == -1)
	die("failed to connect to the socket");

    for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
	if (i + 1 >= MESSAGE_SIZE / sizeof *emit) {
	    fputs("warning: message exceeds max size\n", stderr);
	    break;
	}
	strcpy(emit + len, buf);
	len += strlen(buf);
    }

    if (send(sock_fd, emit, MESSAGE_SIZE, 0) == -1)
	die("Failed to send data");
}
