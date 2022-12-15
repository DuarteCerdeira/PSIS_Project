/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/un.h>

/* Local libraries */
#include "../chase.h"

static int n_bots;

int main(int argc, char *argv[])
{
	n_bots = atoi(argv[2]);
	// Check arguments and its restrictions
	if (argc != 3 && (n_bots < 1 || n_bots > 10))
	{
		printf("Usage: %s <server_socket> <number_of_bots [1,10]>", argv[0]);
		exit(-1);
	}

	int sock_fd;
	sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock_fd < 0)
	{
		perror("socket");
		exit(-1);
	}

	// Connect to server
	struct sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	memset(server_addr.sun_path, 0, sizeof(server_addr.sun_path));
	strcpy(server_addr.sun_path, argv[1]); // If cmd arg is the wrong path, messages won't be sent successfully

	int n_bytes = 0;
	struct msg_data msg = {0};
	msg.player_id = BOTS_ID - 1; // players' id is based on PID, whose max is INT16_MAX
	msg.type = CONN;

	srand(time(NULL));
	for (int i = 0; i < n_bots; i++)
	{
		msg.field[i].pos_x = rand() % (WINDOW_SIZE - 1) + 1;
		msg.field[i].pos_y = rand() % (WINDOW_SIZE - 1) + 1;
		msg.field[i].hp = INIT_HP;
		msg.field[i].ch = '*';
	}

	// Send connection message to server
	n_bytes = sendto(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (n_bytes < 0)
	{
		perror("sendto (CONN)");
		exit(-1);
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
			msg.player_id = BOTS_ID + i;
			// The server knows IDs > INT16_MAX are bots
			// and that INT16_MAX + 1 is a "tag" to the connect bots msg above
			n_bytes = sendto(sock_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
			if (n_bytes < 0)
			{
				perror("sendto (BMOV)");
				exit(-1);
			}
			printf("Bot %d sent Move %d\n", i + 1, msg.dir);
		}
		printf("------------SLEEPING------------\n");
	}

	exit(EXIT_SUCCESS);
}
