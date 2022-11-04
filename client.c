#include <dirent.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/Xlib.h>

#include "common.h"
#include "util.h"
#include "client.h"

int
connect_to_daemon(void) {
    int sock_fd;
    struct sockaddr_un sock_address;
    char dpy_name[64], *dpy_name_ptr;
    DIR *dir;
    struct dirent *de;

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
	report(1, TITLE_STATUS, "could not find display in environment, searching for socket");
	
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

	report(1, TITLE_STATUS, "found socket on display %s", dpy_name_ptr);
    }

    return sock_fd;
}
