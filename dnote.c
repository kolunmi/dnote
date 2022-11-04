#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "common.h"
#include "util.h"
#include "client.h"


void
usage(void) {
    fputs("usage: dnote [OPTS]\n"
	  "	-id [string]		asscociate message with id\n"
	  "	-exp [seconds]		time until expiration; '0' will never expire\n"
	  "	-minw [pixels]		minimum window width\n"
	  "	-center			center text\n"
	  "	-no-center		don't center text\n"
	  "	-loc [0-8]		window location\n"
	  "	-ploc [x] [y]		specify precise location, relative to geometry origin\n"
	  "	-pbar [val] [out of]	construct a progress bar\n"
	  "	-cmd [command]		run shell command when window is selected\n"
	  "	-img [filepath]		render png to window\n"
	  "	-img-header		position png at top of window\n"
	  "	-img-inline		position png next to text\n"
	  "	-imut			remove ability to kill notification by clicking\n"
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
    int itmp1, itmp2;
    unsigned int uitmp1, uitmp2;
    float ftmp;
    int sock_fd;

    emit[0] = DNOTE_PREFIX;
    len = 1;
    
    for (i = 1; i < argc; i++) {
	/* these options take no arguments */
	if (!strcmp(argv[i], "-v")) {
	    puts("dnote-"VERSION);
	    exit(0);
	}
	else if (!strcmp(argv[i], "-center")) {
	    snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_JUSTIFY_CENTER);
	}
	else if (!strcmp(argv[i], "-no-center")) {
	    snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_JUSTIFY_LEFT);
	}
	else if (!strcmp(argv[i], "-img-header")) {
	    snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_HEADER_IMAGE);
	}
	else if (!strcmp(argv[i], "-img-inline")) {
	    snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_INLINE_IMAGE);
	}
	else if (!strcmp(argv[i], "-imut")) {
	    snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_IMMUTABLE);
	}
	else if (i + 1 == argc) {
	    usage();
	}
	/* these options take 1 argument */
	else if (!strcmp(argv[i], "-minw")) {
	    uitmp1 = atoi(argv[++i]);
	    snprintf(buf, sizeof buf, "%c%i", DNOTE_OPTION_MIN_WIDTH, uitmp1);
	}
	else if (!strcmp(argv[i], "-exp")) {
	    ftmp = atof(argv[++i]);
	    if (ftmp)
		snprintf(buf, sizeof buf, "%c%f", DNOTE_OPTION_EXPIRE, ftmp);
	    else
		snprintf(buf, sizeof buf, "%c", DNOTE_OPTION_NO_EXPIRE);
	}
	else if (!strcmp(argv[i], "-loc")) {
	    uitmp1 = atoi(argv[++i]);
	    if (uitmp1 > 8)
		die("-loc : invalid location specifier");
	    snprintf(buf, sizeof buf, "%c%i", DNOTE_OPTION_LOCATION, uitmp1);
	}
	else if (!strcmp(argv[i], "-id")) {
	    snprintf(buf, MAX_ID_LEN + 1, "%c%s", DNOTE_OPTION_ID, argv[++i]);
	}
	else if (!strcmp(argv[i], "-cmd")) {
	    snprintf(buf, MAX_SHCMD_LEN + 1, "%c%s", DNOTE_OPTION_SHELL_COMMAND, argv[++i]);
	}
	else if (!strcmp(argv[i], "-img")) {
	    buf[0] = DNOTE_OPTION_IMAGE_PATH;
	    if (realpath(argv[++i], buf + 1) == NULL)
		die("-img : could not find file");
	}
	else if (i + 2 >= argc) {
	    usage();
	}
	/* these options take 2 arguments */
	else if (!strcmp(argv[i], "-pbar")) {
	    uitmp1 = atoi(argv[++i]);
	    uitmp2 = atoi(argv[++i]);
	    if (!uitmp2 || uitmp1 > uitmp2)
		die("-pbar : invalid arguments");
	    snprintf(buf, sizeof buf, "%c%i/%i", DNOTE_OPTION_PROGRESS_BAR, uitmp1, uitmp2); 
	}
	else if (!strcmp(argv[i], "-ploc")) {
	    itmp1 = atoi(argv[++i]);
	    itmp2 = atoi(argv[++i]);
	    snprintf(buf, sizeof buf, "%c%i/%i", DNOTE_OPTION_PRECISE_LOCATION, itmp1, itmp2);
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

    
    sock_fd = connect_to_daemon();

    
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
