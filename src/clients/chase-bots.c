/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/un.h>

/* Local libraries */
#include "../chase.h"

static int n_bots;
// I need these variables to be global to be able to use them in the disconnect function
static int sock_fd;
static struct sockaddr_un bots_addr;
static struct sockaddr_un server_addr;

// Function to deal with CTRL + C as a normal disconnect
void disconnect()
{
	// Send disconnection message to server
	struct msg_data msg = {0};
	msg.player_id = BOTS_ID - 1;
	msg.type = DCONN;
	sendto(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	close(sock_fd);
	unlink(bots_addr.sun_path);
	exit(EXIT_SUCCESS);
}

// Function to catch CTRL + C
void sigint_handler(int signum)
{
	disconnect();
}

int main(int argc, char *argv[])
{
	// Register CTRL + C handler
	signal(SIGINT, sigint_handler);

	// Check arguments and its restrictions
	if (argc != 3)
	{
		printf("Usage: %s <server_socket> <number_of_bots [1,10]>", argv[0]);
		exit(-1);
	}
	else if ((n_bots = atoi(argv[2])) < 1 || n_bots > 10)
	{
		printf("Bots number must be an integer in range [1,10]\n");
		exit(-1);
	}

	sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock_fd < 0)
	{
		perror("socket: ");
		exit(-1);
	}

	bots_addr.sun_family = AF_UNIX;
	memset(bots_addr.sun_path, 0, sizeof(bots_addr.sun_path));
	sprintf(bots_addr.sun_path, "%s-%s", SOCKET_PREFIX, "bots");

	unlink(bots_addr.sun_path);

	int err = bind(sock_fd, (struct sockaddr *)&bots_addr, sizeof(bots_addr));
	if (err < 0)
	{
		perror("bind: ");
		exit(-1);
	}

	// Connect to server
	server_addr.sun_family = AF_UNIX;
	memset(server_addr.sun_path, 0, sizeof(server_addr.sun_path));
	strcpy(server_addr.sun_path, argv[1]); // If cmd arg is the wrong path, messages won't be sent successfully

	int n_bytes = 0;
	struct msg_data msg = {0};
	msg.player_id = BOTS_ID - 1; // players' id is based on PID, whose max is INT32_MAX
	msg.type = CONN;

	srand(time(NULL));
	for (int i = 0; i < n_bots; i++)
	{
		msg.field[i].pos_x = rand() % (WINDOW_SIZE - 2) + 1;
		msg.field[i].pos_y = rand() % (WINDOW_SIZE - 2) + 1;
		msg.field[i].hp = MAX_HP;
		msg.field[i].ch = '*';
	}

	// Send connection message to server
	n_bytes = sendto(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (n_bytes < 0)
	{
		perror("sendto (CONN)");
		disconnect();
	}

	// Receive connection confirmation from server
	struct sockaddr_un recv_addr;
	socklen_t recv_addr_len = sizeof(recv_addr);
	n_bytes = recvfrom(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
	if (n_bytes < 0)
	{
		perror("connect recvfrom: ");
		disconnect();
	}
	else if (strcmp(recv_addr.sun_path, server_addr.sun_path) != 0)
	{
		printf("Connect confirmation is not from server\n");
		disconnect();
	}
	else if (msg.type == CONN)
	{
		printf("Connected to server\n");
	}
	else
	{
		printf("Unexpected message type: %d\n", msg.type);
		disconnect();
	}

	while (1)
	{
		sleep(3);
		for (int i = 0; i < n_bots; i++)
		{
			msg = (struct msg_data){0};
			msg.type = BMOV;
			msg.dir = rand() % 4 + 1;
			// We'll identify bots by IDs players can't have, with numbers over INT16_MAX
			msg.player_id = (long)BOTS_ID + i;
			// The server knows IDs > INT16_MAX are bots
			// and that INT16_MAX + 1 is a "tag" to the connect bots msg above
			n_bytes = sendto(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (n_bytes < 0)
			{
				perror("sendto (BMOV)");
				exit(-1);
			}
			printf("Bot %d sent Move %d\n", i + 1, msg.dir);

			n_bytes = recvfrom(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
			if (n_bytes < 0)
			{
				perror("recvfrom (BMOV)");
				exit(-1);
			}
			else if (strcmp(recv_addr.sun_path, server_addr.sun_path) != 0)
			{
				printf("Move response is not from server\n");
				exit(-1);
			}
			else if (msg.type == BMOV)
			{
				printf("Bot move successful\n\n");
			}
		}
		printf("------------SLEEPING------------\n");
	}
	disconnect();
}
