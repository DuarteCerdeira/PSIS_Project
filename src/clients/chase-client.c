/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/un.h>

/* Local libraries */
#include "../chase.h"

typedef struct player_t
{
	int pos_x, pos_y;
	int hp;
	char ch;
} player_t;

// Needed these to be global to be able to use them in the disconnect function
static int client_socket;
static struct sockaddr_un client_address;
static struct sockaddr_un server_address;

player_t player;
int client_id;

void disconnect(int send_msg)
{
	// The argument is used to know if we want to send a message to the server
	// In the HP0 case we don't, but we do in any other case
	// Even in the CTRL + C case
	if (send_msg)
	{
		// Send a DCONN message to the server
		struct msg_data msg = {0};
		msg.player_id = client_id;
		msg.type = DCONN;
		sendto(client_socket, &msg, sizeof(msg), 0, (struct sockaddr *)&server_address, sizeof(server_address));
	}
	close(client_socket);
	unlink(client_address.sun_path);
	endwin();
	exit(0);
}

// Function to deal with CTRL + C as a normal disconnect
void sigint_handler(int signum)
{
	disconnect(true);
}

void new_player(player_t *player, ball_info_t p_stats)
{
	player->pos_x = p_stats.pos_x;
	player->pos_y = p_stats.pos_y;
	player->hp = p_stats.hp;
	player->ch = p_stats.ch;
	return;
}

direction_t get_direction(player_t *player, int direction)
{
	switch (direction)
	{
	case KEY_UP:
		return UP;
		break;
	case KEY_DOWN:
		return DOWN;
		break;
	case KEY_LEFT:
		return LEFT;
		break;
	case KEY_RIGHT:
		return RIGHT;
		break;
	default:
		return NONE;
	}
}

void draw_field(WINDOW *game_win, WINDOW *msg_win, ball_info_t *players)
{
	werase(game_win);
	werase(msg_win);
	box(game_win, 0, 0);
	box(msg_win, 0, 0);
	for (int i = 0; i < 3 * MAX_PLAYERS; i++)
	{
		if (players[i].ch == 0)
			continue;
		// draw player
		mvwaddch(game_win, players[i].pos_y, players[i].pos_x, players[i].ch);
		mvwprintw(msg_win, i + 1, 1, "%c %d", players[i].ch, players[i].hp);
	}

	wrefresh(msg_win);
	wrefresh(game_win);
	return;
}

void write_string(WINDOW *win, char *str)
{
	werase(win);
	box(win, 0, 0);
	mvwprintw(win, 1, 1, "%s", str);
	wrefresh(win);
}

int main(int argc, char *argv[])
{
	// get server address
	if (argc < 2)
	{
		printf("Usage: %s <server_address>\n", argv[0]);
		exit(-1);
	}

	// We want to catch
	signal(SIGINT, sigint_handler);

	client_id = getpid();
	// open socket
	client_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (client_socket == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	// bind address

	client_address.sun_family = AF_UNIX;
	memset(client_address.sun_path, '\0', sizeof(client_address.sun_path));
	sprintf(client_address.sun_path, "%s-%d", SOCKET_PREFIX, client_id);

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
	server_address.sun_family = AF_UNIX;
	memset(server_address.sun_path, '\0', sizeof(server_address.sun_path));
	strcpy(server_address.sun_path, argv[1]);

	// send connect message
	struct msg_data connect_msg = {0};
	connect_msg.type = CONN;
	connect_msg.player_id = client_id;

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
			exit(0);
		else if (connect_msg.type == BINFO)
			break;
	}

	// create player
	new_player(&player, connect_msg.field[0]);
	mvwaddch(player_win, player.pos_y, player.pos_x, player.ch);
	wrefresh(player_win);

	int key = -1;
	struct msg_data msg;

	while (key != 27 && key != 'q')
	{
		// send move message
		msg = (struct msg_data){0}; // clear message
		msg.type = BMOV;
		msg.player_id = client_id;

		key = wgetch(player_win);
		if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)
			msg.dir = get_direction(&player, key);
		else if (key == 27 || key == 'q')
			break;
		else
			continue;
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
							   &recv_address_len);

			// checking that the message is from the server
			if (n_bytes == -1)
			{
				perror("Field status recvfrom: ");
				exit(-1);
			}
			else if (strcmp(recv_address.sun_path, server_address.sun_path) != 0)
				continue;
			else if (msg.type == FSTATUS || msg.type == HP0)
				break;
		}

		if (msg.type == HP0)
		{
			write_string(msg_win, "You died\nExiting...\n");
			disconnect(false);
		}
		else if (msg.type == FSTATUS)
		{
			draw_field(player_win, msg_win, msg.field);
		}
	}

	write_string(msg_win, "Disconnected\nExiting...\n");
	disconnect(true);
	sleep(3);

	exit(0);
}
