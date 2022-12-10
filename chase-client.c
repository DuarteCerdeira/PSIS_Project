/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/un.h>

/* Local libraries */
#include "chase.h"

int main()
{
    // open socket
    int client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (client_socket == -1) {
	perror("socket");
	exit(-1);
    }

    // bind address
    struct sockaddr_un client_address;
    client_address.sun_family = AF_UNIX;
    sprintf(client_address.sun_path, "%s-%d", SOCKET_PREFIX, getpid());

    unlink(client_address.sun_path);

    int err = bind(client_socket,
		   (struct sockaddr *) &client_address,
		   sizeof(client_address));
    if (err == -1) {
	perror("bind");
	exit(-1);
    }

    // server address
    struct sockaddr_un server_address;
    server_address.sun_family = AF_UNIX;
    sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

    exit(0);
}
