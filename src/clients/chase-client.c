/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* NCurses */
#include <ncurses.h>

/* Threads */
#include <pthread.h>

/* System libraries */
#include <sys/socket.h>
#include <arpa/inet.h>

/* Local libraries */
#include "../chase.h"

/* Global variables */
static int client_socket;
static WINDOW *game_win;
static WINDOW *stats_win;
static struct sockaddr_in server_address;
pthread_mutex_t win_mtx = PTHREAD_MUTEX_INITIALIZER;

long client_id;

void disconnect()
{
	// The argument is used to know if we want to send a message to the server
	// In the HP0 case we don't, but we do in any other case
	// Even in the CTRL + C case

	close(client_socket);
	endwin();
	exit(0);
}

// Function to deal with CTRL + C as a normal disconnect
void sigint_handler(int signum) { disconnect(); }

direction_t get_direction(int direction)
{
	// Translate the key pressed to a direction
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

// Thread function to recieve field status msgs from server
void *recv_field(void *arg)
{
	int nbytes = 0;
	char buffer[sizeof(struct msg_data)] = {0};
	struct msg_data msg;

	while (1)
	{
		nbytes = 0;
		msg = (struct msg_data){0};
		memset(buffer, 0, sizeof(buffer));
		// This guarantees all the data is received using socket streams
		do
		{
			char *ptr = &buffer[nbytes];
			nbytes += recv(client_socket, ptr, sizeof(buffer) - nbytes, 0);
			if (nbytes == -1)
			{
				perror("recv: ");
				exit(-1);
			}
		} while (nbytes < sizeof(struct msg_data) && nbytes != 0);

		if (nbytes <= 0)
		{
			disconnect();
		}

		memcpy(&msg, buffer, sizeof(struct msg_data));

		if (msg.type == FSTATUS)
		{
			/* == Critical Region == */
			pthread_mutex_lock(&win_mtx);
			// Print the field
			update_field(game_win, msg.field);
			update_stats(stats_win, msg.field);
			wrefresh(game_win);
			wrefresh(stats_win);
			pthread_mutex_unlock(&win_mtx);
			/* ===================== */
		}
		else if (msg.type == HP0) // TODO: Continue Game stuff [A]
		{
			// If the player has 0 HP, disconnect
			disconnect();
		}
	}
}

int main(int argc, char *argv[])
{
	int sock_port = 0;
	// get server address and port from command line
	if (argc < 3)
	{
		printf("Usage: %s <server_address> <server_port>\n", argv[0]);
		exit(-1);
	}
	else if (inet_addr(argv[1]) == INADDR_NONE)
	{
		printf("Invalid server address\n");
		exit(-1);
	}
	else if ((sock_port = atoi(argv[2])) < 1024 || sock_port > 65535)
	{
		printf("Invalid server port\n");
		exit(-1);
	}

	// We want to catch
	signal(SIGINT, sigint_handler);

	client_id = (long)getpid();

	// open socket
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	// No need to bind address with stream sockets

	// server address
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(sock_port);
	if (inet_pton(AF_INET, argv[1], &server_address.sin_addr) < 1)
	{
		printf("Invalid network address\n");
		exit(-1);
	}

	// Send connection request
	if (connect(client_socket, (struct sockaddr *)&server_address,
				sizeof(server_address)) == -1)
	{
		perror("connect: ");
		exit(-1);
	}

	// Ncurses initialization
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	// Create the game window
	game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);
	keypad(game_win, TRUE);

	// Create the message window
	stats_win = newwin(MAX_PLAYERS, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(stats_win, 0, 0);
	wrefresh(stats_win);

	struct msg_data msg;
	msg.type = CONN;

	// Receive initial message from server
	int nbytes = 0;
	char buffer[sizeof(struct msg_data)] = {0};

	// Send message to client
	nbytes = 0;
	memset(buffer, 0, sizeof(struct msg_data));

	memcpy(buffer, &msg, sizeof(struct msg_data));

	do
	{
		char *ptr = &buffer[nbytes];
		nbytes += send(client_socket, ptr, sizeof(buffer) - nbytes, 0);
	} while (nbytes < sizeof(struct msg_data));

	nbytes = 0;
	memset(buffer, 0, sizeof(struct msg_data));

	// This guarantees all the data is received using socket streams
	do
	{
		char *ptr = &buffer[nbytes];
		nbytes += recv(client_socket, ptr, sizeof(buffer) - nbytes, 0);
		if (nbytes == -1)
		{
			perror("recv: ");
			disconnect();
		}
	} while (nbytes < sizeof(struct msg_data) && nbytes != 0);

	if (nbytes == 0)
		disconnect();

	memcpy(&msg, buffer, sizeof(struct msg_data));

	if (msg.type == BINFO)
	{
		// create player
		/* mvwaddch(game_win, msg.field[0].pos_y, msg.field[0].pos_x, msg.field[0].ch); */
		/* wrefresh(game_win); */

		update_field(game_win, msg.field);
		update_stats(stats_win, msg.field);
		wrefresh(game_win);
		wrefresh(stats_win);
	}
	else
		disconnect();

	// Create thread to receive field status messages
	pthread_t recv_thread;
	pthread_mutex_init(&win_mtx, NULL);
	pthread_create(&recv_thread, NULL, recv_field, NULL);

	int key = -1;

	// Read key and send movement messages to server
	while (1)
	{
		msg = (struct msg_data){0};
		msg.type = BMOV;

		// Read key input
		key = wgetch(game_win);
		if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)
			msg.dir = get_direction(key);
		else if (key == 27 || key == 'q')
			break;
		else
			continue;

		// Send movement message to server
		nbytes = 0;
		memset(buffer, 0, sizeof(struct msg_data));
		memcpy(buffer, &msg, sizeof(struct msg_data));
		do
		{
			char *ptr = &buffer[nbytes];
			nbytes += send(client_socket, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data));
	}

	disconnect();
}
