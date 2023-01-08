/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <arpa/inet.h>

/* Local libraries */
#include "../chase.h"

#define INIT_X WINDOW_SIZE / 2 // Initial x position
#define INIT_Y WINDOW_SIZE / 2 // Initial y position

#define SOCK_PORT 40000

/* Client information structure */
struct client_info
{
	long id;
	ball_info_t info;
};

/* Global variables */
static struct client_info players[MAX_PLAYERS];
static struct client_info bots[MAX_PLAYERS];
static struct client_info prizes[MAX_PLAYERS];

static char active_chars[MAX_PLAYERS];

static int active_players;
static int active_prizes;

static long board_grid[WINDOW_SIZE][WINDOW_SIZE];

struct client_info *select_ball(long id)
{
	int i;
	struct client_info *ball;
	
	// Ball is a prize
	if (id < 0)
	{
		ball = &prizes[-id - 1];
	}
	
	// Ball is a bot
	else if (id >= BOTS_ID)
	{
		// Are the bots IDs sequential??
		// that would make this way simpler [D]
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (bots[i].id == id)
				break;
		}
		ball = (i == MAX_PLAYERS) ? NULL : &bots[i];
	}
	
	// Ball is a player
	else
	{
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (players[i].id == id)
				break;
		}
		ball = (i == MAX_PLAYERS) ? NULL : &players[i];
	}

	return ball;
}

struct client_info *check_collision(int x, int y, long id)
{
	// Checks wether the position is occupied and
	// returns the ball in that position
	return board_grid[x][y] != 0 ? select_ball(board_grid[x][y]) : NULL;
}

void field_status(ball_info_t *field)
{
	int j = 0;
	
	// Fills out players information
	for (int i = 0; i < active_players; i++)
	{
		field[i].ch = players[i].info.ch;
		field[i].hp = players[i].info.hp;
		field[i].pos_x = players[i].info.pos_x;
		field[i].pos_y = players[i].info.pos_y;
		j++;
	}

	// Fills out bots information
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (bots[i].id == 0)
			continue;
		field[j].ch = bots[i].info.ch;
		field[j].hp = bots[i].info.hp;
		field[j].pos_x = bots[i].info.pos_x;
		field[j].pos_y = bots[i].info.pos_y;
		j++;
	}

	// Fills out prizes information
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (prizes[i].id != 0)
		{
			field[j].ch = prizes[i].info.ch;
			field[j].hp = prizes[i].info.hp;
			field[j].pos_x = prizes[i].info.pos_x;
			field[j].pos_y = prizes[i].info.pos_y;
			j++;
		}
	}
	return;
}


void print_player_stats(WINDOW *win)
{
	ball_info_t p_stats[MAX_PLAYERS] = {0};
	for (size_t i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i].id == 0)
			continue;
		
		p_stats[i].ch = players[i].info.ch;
		p_stats[i].hp = players[i].info.hp;
		p_stats[i].pos_x = players[i].info.pos_x;
		p_stats[i].pos_y = players[i].info.pos_y;
	}

	update_stats(win, p_stats);
	return;
}

void create_prizes(WINDOW *win)
{
	srand(time(NULL));
	for (int i = 0; i < 5 && active_prizes < 10; i++, active_prizes++)
	{
		int x;
		int y;

		// Generate a random position that is not occupied
		do
		{
			x = rand() % (WINDOW_SIZE - 2) + 1;
			y = rand() % (WINDOW_SIZE - 2) + 1;
		} while (board_grid[x][y] != 0);

		// Generate a prize with a random value between 1 and 5
		int value = rand() % 5 + 1;
		prizes[active_prizes].info.pos_x = x;
		prizes[active_prizes].info.pos_y = y;
		prizes[active_prizes].info.ch = value + '0';
		prizes[active_prizes].info.hp = value;
		prizes[active_prizes].id = -(active_prizes + 1);

		// Add prize to the board
		board_grid[x][y] = prizes[active_prizes].id;
		add_ball(win, &prizes[active_prizes].info);
	}
}

int main()
{
	// Open socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1)
	{
		perror("socket");
		exit(-1);
	}
	
	// Bind address
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SOCK_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (struct sockaddr *) &server_address,
			 sizeof(server_address)) == -1) {
		perror("bind");
		exit(-1);
	}

	if (listen(server_socket, 10) == -1) {
		perror("listen");
		exit(-1);
	}

	// Ncurses initialization
	initscr();
	cbreak();
	noecho();

	// Create the game window
	WINDOW *game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);

	// Create the message window
	WINDOW *msg_win = newwin(MAX_PLAYERS, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	// Store client information
	struct sockaddr_in client_address;
	socklen_t client_address_size;
	
	// Initialize global variables
	active_players = 0;
	active_prizes = 0;
	
	memset(active_chars, 0, sizeof(active_chars));
	
	memset(players, 0, sizeof(players));
	memset(bots, 0, sizeof(bots));
	memset(prizes, 0, sizeof(prizes));

	memset(board_grid, 0, sizeof(board_grid));
	while (1) {
		// Wait for connection requests from clients
		memset(&client_address, 0, sizeof(client_address));
		client_address_size = sizeof(struct sockaddr_in);

		int c_fd = accept(server_socket, (struct sockaddr *) &client_address,
							   &client_address_size);

		if (c_fd == -1) {
			perror("accept");
			exit(-1);
		}

		if (active_players >= MAX_PLAYERS) {
			close(c_fd);
			continue;
		}

		/* Create threads for each client */

	}
}
