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

/* Global variables */
player_info_t players[10];
player_info_t bots[10];
int n_players;
char active_chars[10];

int handle_connection(struct sockaddr_un client)
{
	if (n_players >= 10)
	{
		return -1;
	}

	// extract the client pid from the address
	char *client_address = client.sun_path;
	char *client_address_pid = strrchr(client_address, '-') + 1;

	// select a random character to assign to the player
	char rand_char;
	srand(time(NULL)); // added srand() to generate random numbers [Adr]
	do
	{
		rand_char = rand() % ('Z' - 'A') + 'A';
	} while (strchr(active_chars, rand_char) != NULL);

	players[n_players].id = atoi(client_address_pid);
	players[n_players].ch = rand_char;
	players[n_players].hp = 5; // init HP was at 10 (wrong), changed it to 5 [Adr]
	players[n_players].pos_x = WINDOW_SIZE / 2;
	players[n_players].pos_y = WINDOW_SIZE / 2;
	n_players++;
	return 0;
}

void handle_disconnection(int id)
{
	int i;
	for (i = 0; i < n_players; i++)
	{
		if (players[i].id == id)
		{
			break;
		}
	}

	players[i].id = 0;
	players[i].ch = '\0';
	players[i].hp = 0;
	players[i].pos_x = 0;
	players[i].pos_y = 0;
}

int check_collision(int id, int x, int y)
{
	int i;
	for (i = 0; i < n_players; i++)
	{
		if (players[i].pos_x == x && players[i].pos_y == y && players[i].id != id)
		{
			return i;
		}
	}

	return -1;
}

int handle_move(int id, direction_t dir)
{
	int i;
	for (i = 0; i < n_players; i++)
	{
		if (players[i].id == id)
		{
			break;
		}
	}

	if (i == n_players)
	{
		return -1;
	}

	if (players[i].hp == 0)
	{
		return 0;
	}

	switch (dir)
	{
	case UP:
		if (players[i].pos_y > 1)
		{
			players[i].pos_y--;
		}
		break;
	case DOWN:
		if (players[i].pos_y < WINDOW_SIZE - 1)
		{
			players[i].pos_y++;
		}
		break;
	case LEFT:
		if (players[i].pos_x > 1)
		{
			players[i].pos_x--;
		}
		break;
	case RIGHT:
		if (players[i].pos_x < WINDOW_SIZE - 1)
		{
			players[i].pos_x++;
		}
		break;
	default:
		break;
	}

	int coll = check_collision(players[i].id, players[i].pos_x, players[i].pos_y);
	if (coll != -1)
	{
		players[i].hp == 10 ? players[i].hp : players[i].hp++;
		players[coll].hp--;
	}

	return 1;
}

int main()
{
	// open socket
	int server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (server_socket == -1)
	{
		perror("socket");
		exit(-1);
	}

	// bind address
	struct sockaddr_un server_address;
	server_address.sun_family = AF_UNIX;
	memset(server_address.sun_path, '\0', sizeof(server_address.sun_path));
	sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

	unlink(server_address.sun_path);

	int err = bind(server_socket,
				   (struct sockaddr *)&server_address,
				   sizeof(server_address));
	if (err == -1)
	{
		perror("bind");
		exit(-1);
	}

	// ncurses initialization
	initscr();
	cbreak();
	noecho();

	// create the game window
	WINDOW *game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);

	// create the message window
	WINDOW *msg_win = newwin(10, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	// store client information
	struct sockaddr_un client_address;
	socklen_t client_address_size = sizeof(client_address);
	memset(client_address.sun_path, '\0', sizeof(client_address.sun_path));

	while (1)
	{
		// wait for messages from clients
		struct msg_data msg = {0}; // reset msg
		int nbytes = recvfrom(server_socket, &msg, sizeof(msg), 0,
							  (struct sockaddr *)&client_address,
							  &client_address_size);

		if (nbytes == -1)
		{
			perror("recvfrom");
			exit(-1);
		}
		if (nbytes == 0)
		{
			msg.type = DCONN;
		}

		switch (msg.type)
		{
		case (CONN):
			if (handle_connection(client_address) == -1)
			{
				msg.type = RJCT;
			}
			else
			{
				msg = (struct msg_data){0}; // reset msg
				msg.type = BINFO;
				msg.ch = players[n_players - 1].ch;
				msg.player_id = players[n_players - 1].id;
				msg.hp = players[n_players - 1].hp;
			}
			break;
		case (DCONN):
			handle_disconnection(msg.player_id);
			n_players--;
			break;
		case (BMOV):
		{
			int alive = handle_move(msg.player_id, msg.dir);
			msg = (struct msg_data){0}; // reset msg
			if (alive == -1)
			{
				continue;
			}
			if (alive)
			{
				msg.type = FSTATUS;
				break;
			}
			msg.type = HP0;
			n_players--;
			break;
		}
		default:
			continue;
		}

		sendto(server_socket, &msg, sizeof(msg), 0,
			   (struct sockaddr *)&client_address,
			   client_address_size);

		wrefresh(msg_win);
	}
	endwin();
	exit(0);
}
