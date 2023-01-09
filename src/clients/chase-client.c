/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <arpa/inet.h>

/* Local libraries */
#include "../chase.h"

/* Global variables */
static int client_socket;
// static struct sockaddr_in client_address;
static struct sockaddr_in server_address;

long client_id;

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
	endwin();
	exit(0);
}

// Function to deal with CTRL + C as a normal disconnect
void sigint_handler(int signum) { disconnect(true); }

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

void write_string(WINDOW *win, char *str)
{
	// Just a function to write a string into the msg window
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

	client_id = (long)getpid();
	
	// open socket
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	// No need to bind address with stream sockets

	#define SOCK_PORT 40000
	
	// server address
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SOCK_PORT);
	if (inet_pton(AF_INET, argv[1], &server_address.sin_addr) < 1) {
		printf("Invalid network address\n");
		exit(-1);
	}

	// Send connection request
	if (connect(client_socket, (struct sockaddr *) &server_address,
				sizeof(server_address)) == -1) {
		perror("connect");
		exit(-1);
	}

	// Ncurses initialization
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	// Create the game window
	WINDOW *game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);
	keypad(game_win, TRUE);

	// Create the message window
	WINDOW *msg_win = newwin(MAX_PLAYERS, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	struct msg_data msg;
	msg.type = CONN;

	// Receive initial message from server
	int nbytes = 0;
	char buffer[sizeof(struct msg_data)] = {0};

	// Send message to client
	nbytes = 0;
	memset(buffer, 0, sizeof(struct msg_data));

	memcpy(buffer, &msg, sizeof(struct msg_data));
		
	do {
		char *ptr = &buffer[nbytes];
		nbytes += send(client_socket, ptr, sizeof(buffer) - nbytes, 0);
	} while (nbytes < sizeof(struct msg_data));

	nbytes = 0;
	memset(buffer, 0, sizeof(struct msg_data));

	// This guarantees all the data is received using socket streams
	do {
		char *ptr = &buffer[nbytes];
		nbytes += recv(client_socket, ptr, sizeof(buffer) - nbytes, 0);
		if (nbytes == -1) {
			perror("recv");
			exit(-1);
		}
	} while (nbytes < sizeof(struct msg_data) && nbytes != 0);

	if (nbytes == 0) {
		write_string(msg_win, "Rejected\n");
		disconnect(false);
		exit(-1);
	}

	memcpy(&msg, buffer, sizeof(struct msg_data));

	// create player
	mvwaddch(game_win, msg.field[0].pos_y, msg.field[0].pos_x, msg.field[0].ch);
	wrefresh(game_win);

	sleep(20);
	
	write_string(msg_win, "Disconnected\nExiting...\n");
	disconnect(true);
	sleep(3);

	exit(0);
}
