#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "util.h"
#include "client.h"


void
usage(void) {
    fputs("usage: dnotec [OPTS]\n"
	  "	-list			list active non-anonymous notifications\n"
	  "	-kill [ID]		kill notification with id\n"
	  "	-clear			kill all active notifications\n"
	  "	-renew [ID]		renew notification with id\n"
	  "	-select [ID]		select notification with id\n"
	  "	-img-list		list pngs contained in memory\n"
	  "	-img-load [PATH]	load or reload a png into memory\n"
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
    int sock_fd, nb;

    emit[0] = DNOTEC_PREFIX;
    len = 1;

    for (i = 1; i < argc; i++) {
	/* these options take no arguments */
	if (!strcmp(argv[i], "-v")) {
	    puts("dnotec-"VERSION);
	    exit(0);
	}
	else if (!strcmp(argv[i], "-list")) {
	    snprintf(buf, sizeof buf, "%c", DNOTEC_OPTION_LIST);
	}
	else if (!strcmp(argv[i], "-clear")) {
	    snprintf(buf, sizeof buf, "%c", DNOTEC_OPTION_CLEAR);
	}
	else if (!strcmp(argv[i], "-img-list")) {
	    snprintf(buf, sizeof buf, "%c", DNOTEC_OPTION_IMAGE_LIST);
	}
	else if (i + 1 == argc) {
	    usage();
	}
	/* these options take 1 argument */
	else if (!strcmp(argv[i], "-kill")) {
	    snprintf(buf, MAX_ID_LEN + 1, "%c%s", DNOTEC_OPTION_KILL, argv[++i]);
	}
	else if (!strcmp(argv[i], "-renew")) {
	    snprintf(buf, MAX_ID_LEN + 1, "%c%s", DNOTEC_OPTION_RENEW, argv[++i]);
	}
	else if (!strcmp(argv[i], "-select")) {
	    snprintf(buf, MAX_ID_LEN + 1, "%c%s", DNOTEC_OPTION_SELECT, argv[++i]);
	}
	else if (!strcmp(argv[i], "-img-load")) {
	    buf[0] = DNOTEC_OPTION_IMAGE_LOAD;
	    if (realpath(argv[++i], buf + 1) == NULL)
		die("-img-load : could not find file");
	}
	else if (i + 2 >= argc) {
	    usage();
	}

	tmplen = len + strlen(buf) + 1;
	if (tmplen >= MESSAGE_SIZE)
	    die("message args exceed max size");
	strcpy(emit + len, buf);
	len = tmplen;
    }

    if (len == 1)
	usage();

    
    sock_fd = connect_to_daemon();


    if (send(sock_fd, emit, len, 0) == -1)
	die("failed to send the data");


    struct pollfd fds[] = {
	{sock_fd, POLLIN, 0},
	{STDOUT_FILENO, POLLHUP, 0},
    };

    while (poll(fds, 2, -1) > 0) {
	if (fds[0].revents & POLLIN) {
	    if ((nb = recv(sock_fd, buf, sizeof buf - 1, 0)) > 0) {
		buf[nb] = '\0';
		fprintf(stdout, "%s", buf);
		fflush(stdout);
	    } else {
		break;
	    }
	}
	if (fds[1].revents & (POLLERR | POLLHUP)) {
	    break;
	}
    }
}
