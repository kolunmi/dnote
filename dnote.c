#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    char *dispname;
    
    len = 0;
    
    for (i = 1; i < argc; i++) {
	/* these options take no arguments */
	if (!strcmp(argv[i], "-v")) {
	    puts("dnote-"VERSION);
	    exit(0);
	}
	else if (!strcmp(argv[i], "-c")) {
	    strcpy(buf, "c");
	}
	else if (!strcmp(argv[i], "-nc")) {
	    strcpy(buf, "n");
	}
	else if (i + 1 == argc) {
	    usage();
	}
	/* these options take 1 argument */
	else if (!strcmp(argv[i], "-minw")) {
	    tmp1 = atoi(argv[++i]);
	    snprintf(buf, sizeof buf, "w%i", tmp1);
	}
	else if (!strcmp(argv[i], "-exp")) {
	    tmpf = atof(argv[++i]);
	    if (tmpf)
		snprintf(buf, sizeof buf, "e%f", tmpf);
	    else
		strcpy(buf, "z");
	}
	else if (!strcmp(argv[i], "-loc")) {
	    tmp1 = atoi(argv[++i]);
	    if (tmp1 > 8)
		die("-loc : invalid location specifier");
	    snprintf(buf, sizeof buf, "l%i", tmp1);
	}
	else if (!strcmp(argv[i], "-id")) {
	    i++;
	    tmp1 = strlen(argv[i]);
	    if (tmp1 > MAX_ID_LEN) {
		argv[i][MAX_ID_LEN - 1] = '\0';
		tmp1 = MAX_ID_LEN;
	    }
	    snprintf(buf, sizeof buf, "i%s", argv[i]);
	}
	else if (!strcmp(argv[i], "-cmd")) {
	    snprintf(buf, MAX_SHCMD_LEN, "s%s", argv[++i]);
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
	    snprintf(buf, sizeof buf, "p%i/%i", tmp1, tmp2); 
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
    dispname = XDisplayName(NULL);
    
    if (dispname)
	if (snprintf(sock_address.sun_path, sizeof sock_address.sun_path, SOCKET_PATH, dispname) == -1)
	    die("cannot write the socket path");
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	die("failed to create the socket");
    if (connect(sock_fd, (struct sockaddr *) &sock_address, sizeof(sock_address)) == -1)
	die("failed to connect to the socket");

    
    for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
	tmplen = len + strlen(buf);
	if (tmplen >= MESSAGE_SIZE) {
	    fputs("warning: message exceeds max size; trimming\n", stderr);
	    break;
	}
	strcpy(emit + len, buf);
	len = tmplen;
    }

    if (send(sock_fd, emit, len, 0) == -1)
	die("failed to send the data");
}
