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
#include "../chase.h"

typedef struct player_t
{
	int x, y;
	int hp;
	char c;
} player_t;

void new_player(player_t *player, struct msg_data msg)
{
	player->x = INIT_X;
	player->y = INIT_Y;
	player->hp = msg.hp;
	player->c = msg.ch;
}

direction_t move_player(player_t *player, int direction)
{
	if (player->y != 1 && direction == KEY_UP)
		return UP;

	else if (player->y != WINDOW_SIZE - 2 && direction == KEY_DOWN)
		return DOWN;

	else if (player->x != 1 && direction == KEY_LEFT)
		return LEFT;

	else if (player->x != WINDOW_SIZE - 2 && direction == KEY_RIGHT)
		return RIGHT;

	return -1;
}

player_t player;

int main()
{
	// open socket
	int client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (client_socket == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	// bind address
	struct sockaddr_un client_address;
	client_address.sun_family = AF_UNIX;
	memset(client_address.sun_path, '\0', sizeof(client_address.sun_path));
	sprintf(client_address.sun_path, "%s-%d", SOCKET_PREFIX, getpid());

	unlink(client_address.sun_path);

	int err = bind(client_socket,
				   (struct sockaddr *)&client_address,
				   sizeof(client_address));
	if (err == -1)
	{
		perror("bind: ");
		exit(-1);
	}

	// server address
	struct sockaddr_un server_address;
	server_address.sun_family = AF_UNIX;
	memset(server_address.sun_path, '\0', sizeof(server_address.sun_path));
	sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

	// send connect message
	struct msg_data connect_msg = {0};
	connect_msg.type = CONN;
	connect_msg.player_id = getpid();

	int n_bytes = sendto(client_socket,
						 &connect_msg,
						 sizeof(connect_msg),
						 0,
						 (struct sockaddr *)&server_address,
						 sizeof(server_address));
	if (n_bytes == -1)
	{
		perror("connect sendto: ");
		exit(-1);
	}

	struct sockaddr_un recv_address;
	socklen_t recv_address_len = sizeof(recv_address);
	recv_address.sun_family = AF_UNIX;
	memset(recv_address.sun_path, '\0', sizeof(recv_address.sun_path));

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	WINDOW *player_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(player_win, 0, 0);
	wrefresh(player_win);
	keypad(player_win, TRUE);

	// message window
	WINDOW *msg_win = newwin(5, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	while (1)
	{
		memset(&connect_msg, 0, sizeof(connect_msg));
		// receive connect message
		n_bytes = recvfrom(client_socket,
						   &connect_msg,
						   sizeof(connect_msg),
						   0,
						   (struct sockaddr *)&recv_address,
						   &recv_address_len);

		// checking that the message is from the server
		if (n_bytes == -1)
		{
			perror("connect recvfrom: ");
			exit(-1);
		}
		else if (strcmp(server_address.sun_path, recv_address.sun_path) != 0)
			continue;

		// checking message type
		if (connect_msg.type == RJCT)
		{
			werase(msg_win);
			box(msg_win, 0, 0);
			mvwprintw(msg_win, 1, 1, "Server is full\n");
			wrefresh(msg_win);
			exit(0);
		}
		else if (connect_msg.type == BINFO)
		{
			werase(msg_win);
			box(msg_win, 0, 0);
			mvwprintw(msg_win, 1, 1, "Connected to server\n");
			wrefresh(msg_win);
			break;
		}
	}

	// create player
	new_player(&player, connect_msg);

	int key = -1;
	struct msg_data msg;

	while (key != 27 && key != 'q')
	{
		// send move message
		msg = (struct msg_data){0}; // clear message
		msg.type = BMOV;
		msg.player_id = getpid();
		werase(msg_win);
		box(msg_win, 0, 0);
		mvwprintw(msg_win, 1, 1, "Waiting for key\n");
		wrefresh(msg_win);

		key = wgetch(player_win);
		if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)
			msg.dir = move_player(&player, key);

		n_bytes = sendto(client_socket,
						 &msg,
						 sizeof(msg),
						 0,
						 (struct sockaddr *)&server_address,
						 sizeof(server_address));
		if (n_bytes == -1)
		{
			perror("Ball move sendto: ");
			exit(-1);
		}
		werase(msg_win);
		box(msg_win, 0, 0);
		mvwprintw(msg_win, 1, 1, "Sent move message\n");
		wrefresh(msg_win);

		// receive field status
		while (1)
		{
			n_bytes = recvfrom(client_socket,
							   &msg,
							   sizeof(msg),
							   0,
							   (struct sockaddr *)&recv_address,
							   &recv_address_len);

			// checking that the message is from the server
			if (n_bytes == -1)
			{
				perror("Field status recvfrom: ");
				exit(-1);
			}
			else if (strcmp(recv_address.sun_path, server_address.sun_path) != 0)
				continue;
			else if (msg.type != FSTATUS && msg.type != HP0)
				continue;
		}
		werase(msg_win);
		box(msg_win, 0, 0);
		mvwprintw(msg_win, 1, 1, "Recieved field status\n");
		wrefresh(msg_win);

		if (msg.type == HP0)
		{
			werase(msg_win);
			box(msg_win, 0, 0);
			mvwprintw(msg_win, 1, 1, "You died\nExiting...\n");
			wrefresh(msg_win);
			sleep(5);
			endwin();
			close(client_socket);
			unlink(client_address.sun_path);
			exit(0);
		}
		else if (msg.type == FSTATUS)
		{
			player.hp = msg.hp;
			overwrite(msg.win, player_win);
		}
	}
	endwin();
	close(client_socket);
	unlink(client_address.sun_path);
	exit(0);
}
