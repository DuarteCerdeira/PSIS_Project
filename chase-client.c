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

typedef struct player_t
{
	int x, y;
	int hp;
	char c;
} player_t;

void new_player(player_t *player, char c)
{
	player->x = INIT_X;
	player->y = INIT_Y;
	player->hp = INIT_HP;
	player->c = c;
}

void draw_player(WINDOW *win, player_t *player, int draw)
{
	// draw = 1: draw player
	// draw = 0: erase player
	int ch;
	if (draw)
	{
		ch = player->c;
	}
	else
	{
		ch = ' ';
	}
	int p_x = player->x;
	int p_y = player->y;
	wmove(win, p_y, p_x);
	waddch(win, ch);
	wrefresh(win);
}

void move_player(player_t *player, int direction)
{
	if (player->y != 1 && direction == KEY_UP)
		player->y--;

	else if (player->y != WINDOW_SIZE - 2 && direction == KEY_DOWN)
		player->y++;

	else if (player->x != 1 && direction == KEY_LEFT)
		player->x--;

	else if (player->x != WINDOW_SIZE - 2 && direction == KEY_RIGHT)
		player->x++;

	return;
}

player_t player;

int main()
{
	// open socket
	int client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (client_socket == -1)
	{
		perror("socket");
		exit(-1);
	}

	// bind address
	struct sockaddr_un client_address;
	client_address.sun_family = AF_UNIX;
	sprintf(client_address.sun_path, "%s-%d", SOCKET_PREFIX, getpid());

	unlink(client_address.sun_path);

	int err = bind(client_socket,
				   (struct sockaddr *)&client_address,
				   sizeof(client_address));
	if (err == -1)
	{
		perror("bind");
		exit(-1);
	}

	// server address
	struct sockaddr_un server_address;
	server_address.sun_family = AF_UNIX;
	sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

	// send connect message
	struct msg_data connect_msg;
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
	recv_address.sun_family = AF_UNIX;

	while (1)
	{
		// receive connect message
		n_bytes = recvfrom(client_socket,
						   &connect_msg,
						   sizeof(connect_msg),
						   0,
						   (struct sockaddr *)&recv_address,
						   (size_t *)&recv_address);

		// checking that the message is from the server
		if (n_bytes == -1)
		{
			perror("connect recvfrom: ");
			exit(-1);
		}
		else if (recv_address.sun_path != server_address.sun_path)
		{
			printf("Recieved message is not from server.\nIgnoring...\n");
			continue;
		}

		// checking message type
		if (connect_msg.type == RJCT)
		{
			printf("Server is full.\n");
			exit(0);
		}
		else if (connect_msg.type == BINFO)
		{
			printf("Connected to server.\n");
			break;
		}
	}

	// create player
	new_player(&player, connect_msg.player_id);

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	WINDOW *player_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(player_win, 0, 0);
	wrefresh(player_win);
	keypad(player_win, TRUE);

	// message window
	WINDOW *mesage_win = newwin(5, WINDOW_SIZE, WINDOW_SIZE, 0);
	box(mesage_win, 0, 0);
	wrefresh(mesage_win);

	// draw player
	draw_player(player_win, &player, true);

	int key = -1;
	struct msg_data msg;

	while (key != 27 && key != 'q')
	{
		// send move message
		msg.type = BMOV;
		msg.player_id = player.c;

		key = wgetch(player_win);
		if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)
		{
			draw_player(player_win, &player, false);
			move_player(&player, key);
			draw_player(player_win, &player, true);
		}

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

		// receive field status
		while (1)
		{
			n_bytes = recvfrom(client_socket,
							   &msg,
							   sizeof(msg),
							   0,
							   (struct sockaddr *)&recv_address,
							   (size_t *)&recv_address);

			// checking that the message is from the server
			if (n_bytes == -1)
			{
				perror("Field status recvfrom: ");
				exit(-1);
			}
			else if (recv_address.sun_path != server_address.sun_path)
				continue;
			else if (msg.type != FSTATUS || msg.type != HP0)
				continue;
		}
		if (msg.type == HP0)
		{
			printw(mesage_win, "You died.\n");
			unlink(client_address.sun_path);
			exit(0);
		}
		else if (msg.type == FSTATUS)
		{
			player.hp = msg.hp;
			break;
		}
	}

	exit(0);
}
