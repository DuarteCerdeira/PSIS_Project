#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "chase.h"

int main()
{
    int client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

    char c = getchar();

    struct sockaddr_un server_address;
    server_address.sun_family = AF_UNIX;
    sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

    sendto(client_socket, &c, sizeof(c), 0,
	   (struct sockaddr *) &server_address,
	   sizeof(server_address));

    exit(0);
}
