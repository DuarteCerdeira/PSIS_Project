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
static int server_socket;
static WINDOW *game_win;
static WINDOW *stats_win;
static struct sockaddr_in server_address;
pthread_mutex_t win_mtx = PTHREAD_MUTEX_INITIALIZER;
bool dead = false; // Flag to be triggered when the player dies
pthread_mutex_t dead_mtx = PTHREAD_MUTEX_INITIALIZER;

void disconnect()
{
	// The argument is used to know if we want to send a message to the server
	// In the HP0 case we don't, but we do in any other case
	// Even in the CTRL + C case

	close(server_socket);
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

void update_field(WINDOW *win, ball_info_t players[], int n_players)
{
	for (int i = 0; i < n_players; i++)
	{
		if (players[i].ch == 0) {
			continue;
		}
		mvwaddch(win, players[i].pos_y, players[i].pos_x, players[i].ch);
	}
	wrefresh(win);
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
			nbytes += recv(server_socket, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data) && nbytes > 0);

		if (nbytes <= 0)

			disconnect();

		memcpy(&msg, buffer, sizeof(struct msg_data));

		if (msg.type == FSTATUS)
		{
			// Ignore field status messages if the player is dead
			pthread_mutex_lock(&dead_mtx);
			if (dead)
			{
				pthread_mutex_unlock(&dead_mtx);
				continue;
			}
			pthread_mutex_unlock(&dead_mtx);

			/* == Critical Region == */
			pthread_mutex_lock(&win_mtx);
			
			// Print the field
			update_field(game_win, msg.field, 2);
			update_stats(stats_win, msg.field);
			
			pthread_mutex_unlock(&win_mtx);
			/* ===================== */
		}
		else if (msg.type == HP0)
		{
			/* == Critical Region == */
			pthread_mutex_lock(&dead_mtx);
			dead = true;
			pthread_mutex_unlock(&dead_mtx);

			pthread_mutex_lock(&win_mtx);
			werase(stats_win);
			box(stats_win, 0, 0);
			mvwprintw(stats_win, 1, 1, "You died :/");
			mvwprintw(stats_win, 3, 1, "Keep playing?");
			mvwprintw(stats_win, 5, 1, "Press any key");
			mvwprintw(stats_win, 6, 1, "to continue");
			mvwprintw(stats_win, 7, 1, "in the next 10s");
			wrefresh(stats_win);
			pthread_mutex_unlock(&win_mtx);
			/* ===================== */
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

	// We want to catch CTRL + C
	signal(SIGINT, sigint_handler);

	// open socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1)
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
	if (connect(server_socket, (struct sockaddr *)&server_address,
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
	stats_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, WINDOW_SIZE + 2); // TODO: CHANGE LATER
	box(stats_win, 0, 0);
	wrefresh(stats_win);

	struct msg_data msg;
	msg.type = CONN;

	int nbytes = 0;
	char buffer[sizeof(struct msg_data)] = {0};

	// Send message to server
	nbytes = 0;
	memset(buffer, 0, sizeof(struct msg_data));

	memcpy(buffer, &msg, sizeof(struct msg_data));

	do
	{
		char *ptr = &buffer[nbytes];
		nbytes += send(server_socket, ptr, sizeof(buffer) - nbytes, 0);
	} while (nbytes < sizeof(struct msg_data));

	do {
		// Receive board and BINFO messages from server
		nbytes = 0;
		memset(buffer, 0, sizeof(struct msg_data));
	
		// This guarantees all the data is received using socket streams
		do {
			char *ptr = &buffer[nbytes];
			nbytes += recv(server_socket, ptr, sizeof(buffer) - nbytes, 0);
			if (nbytes == -1)
				{
					perror("recv: ");
					disconnect();
				}
		} while (nbytes < sizeof(struct msg_data) && nbytes != 0);
		
		if (nbytes == 0)
			disconnect();

		memcpy(&msg, buffer, sizeof(struct msg_data));

		update_field(game_win, msg.field, 2);
		update_stats(stats_win, msg.field);
		
	} while (msg.type != BINFO);

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

		// This key check became messier due to the continue game checking
		pthread_mutex_lock(&dead_mtx);
		if (key == 27 || key == 'q')
		{
			pthread_mutex_unlock(&dead_mtx);
			break;
		}
		else if (dead)
		{
			pthread_mutex_lock(&win_mtx);
			werase(stats_win);
			box(stats_win, 0, 0);
			mvwprintw(stats_win, 1, 1, "Reconnecting...");
			wrefresh(stats_win);
			pthread_mutex_unlock(&win_mtx);
			msg.type = CONTGAME;
			dead = false;
		}
		else if (!dead && (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT))
			msg.dir = get_direction(key);
		else
		{
			pthread_mutex_unlock(&dead_mtx);
			continue;
		}
		pthread_mutex_unlock(&dead_mtx);

		// Send movement message to server
		nbytes = 0;
		memset(buffer, 0, sizeof(struct msg_data));
		memcpy(buffer, &msg, sizeof(struct msg_data));
		do
		{
			char *ptr = &buffer[nbytes];
			nbytes += send(server_socket, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data));
	}

	disconnect();
}
