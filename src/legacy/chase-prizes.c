/* Standard Libs*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Ncurses */
#include <ncurses.h>

/* System Libs*/
#include <sys/socket.h>
#include <sys/un.h>

/* Local Libs */
#include "../chase.h"

int main()
{
	// Create socket
	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd == -1)
	{
		perror("socket: ");
		exit(EXIT_FAILURE);
	}

	// Local address
	struct sockaddr_un prizes_addr;
	prizes_addr.sun_family = AF_UNIX;
	memset(prizes_addr.sun_path, 0, sizeof(prizes_addr.sun_path));
	sprintf(prizes_addr.sun_path, "%s-%s", SOCKET_PREFIX, "prizes");

	// Unlink socket
	unlink(prizes_addr.sun_path);

	// Bind socket
	int err = bind(socket_fd, (struct sockaddr *)&prizes_addr, sizeof(prizes_addr));
	if (err == -1)
	{
		perror("bind: ");
		exit(EXIT_FAILURE);
	}

	// Connect to server
	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	memset(server_addr.sun_path, 0, sizeof(server_addr.sun_path));
	sprintf(server_addr.sun_path, "%s-%s", SOCKET_PREFIX, "server");

	int n_bytes = 0;
	struct msg_data msg = {0};
	msg.type = CONN;

	// Receive server feedback
	struct sockaddr_un recv_addr;
	socklen_t recv_addr_len = sizeof(recv_addr);

	while (1)
	{
		n_bytes = sendto(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		if (n_bytes == -1)
		{
			perror("sendto: ");
			close(socket_fd);
			exit(EXIT_FAILURE);
		}

		n_bytes = recvfrom(socket_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
		if (n_bytes == -1)
		{
			perror("recvfrom: ");
			close(socket_fd);
			exit(EXIT_FAILURE);
		}
		else if (strcmp(recv_addr.sun_path, server_addr.sun_path) != 0)
		{
			printf("Received message from unknown source\n");
			close(socket_fd);
			exit(EXIT_FAILURE);
		}
		else if (msg.type != CONN)
		{

			printf("Received message of unknown type\n");
			close(socket_fd);
			exit(EXIT_FAILURE);
		}
		
		sleep(5);
	}
	close(socket_fd);
	exit(EXIT_SUCCESS);
}
