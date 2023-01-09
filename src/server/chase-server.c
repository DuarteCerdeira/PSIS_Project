/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

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

// Windows
WINDOW *game_win = NULL;
WINDOW *msg_win = NULL;

static long board_grid[WINDOW_SIZE][WINDOW_SIZE] = {0};

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

void handle_player_disconnection(WINDOW *win, struct client_info *player)
{
	// Delete the ball from the board
	delete_ball(win, &player->info);
	board_grid[player->info.pos_x][player->info.pos_y] = 0;

	// Delete the character from the list of used characters
	*strrchr(active_chars, player->info.ch) = active_chars[active_players - 1];
	active_chars[active_players - 1] = '\0';

	// Delete player information
	*player = players[active_players - 1];
	memset(&players[active_players - 1], 0, sizeof(struct client_info));

	active_players--;
}

void handle_move(WINDOW *win, struct client_info *ball, direction_t dir)
{
	struct client_info *ball_hit;

	// Check if the position the ball wants to move to is clear
	switch (dir)
	{
	case UP:
		ball_hit = check_collision(ball->info.pos_x, ball->info.pos_y - 1, ball->id);
		break;
	case DOWN:
		ball_hit = check_collision(ball->info.pos_x, ball->info.pos_y + 1, ball->id);
		break;
	case LEFT:
		ball_hit = check_collision(ball->info.pos_x - 1, ball->info.pos_y, ball->id);
		break;
	case RIGHT:
		ball_hit = check_collision(ball->info.pos_x + 1, ball->info.pos_y, ball->id);
		break;
	default:
		break;
	}

	// No ball was hit
	if (ball_hit == NULL)
	{
		// Ball position is updated
		board_grid[ball->info.pos_x][ball->info.pos_y] = 0;
		move_ball(win, &ball->info, dir);
		board_grid[ball->info.pos_x][ball->info.pos_y] = ball->id;
		return;
	}

	// Collision happened - in most cases, the ball doesn't move
	direction_t new_dir = NONE;

	// Player hit a prize
	if (ball_hit->id < 0 && ball->id < BOTS_ID)
	{
		// Player's health is updated
		int health = ball_hit->info.hp;
		ball->info.hp += (ball->info.hp + health > MAX_HP) ? MAX_HP - ball->info.hp : health;

		// Prize is deleted
		struct client_info *last_prize = &prizes[active_prizes - 1];

		last_prize->id = ball_hit->id;
		board_grid[last_prize->info.pos_x][last_prize->info.pos_y] = ball_hit->id;

		*ball_hit = *last_prize;
		memset(last_prize, 0, sizeof(struct client_info));
		active_prizes--;

		// Player position needs to be updated
		new_dir = dir;
	}

	// Ball (player or bot) hit a player
	else if (ball_hit->id < BOTS_ID && ball_hit->id > 0)
	{
		// Ball "steals" 1 HP from the player
		ball->info.hp == MAX_HP ? MAX_HP : ball->info.hp++;
		ball_hit->info.hp == 0 ? 0 : ball_hit->info.hp--;
	}

	board_grid[ball->info.pos_x][ball->info.pos_y] = 0;
	move_ball(win, &ball->info, new_dir);
	board_grid[ball->info.pos_x][ball->info.pos_y] = ball->id;
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

// Thread function that handles bots
void *handle_bots(void *arg)
{
	int n_bots = *(int *)arg;

	// Create bots
	for (int i = 0; i < n_bots; i++)
	{
		// Initialize bots information
		bots[i].id = (long)BOTS_ID + i;
		bots[i].info.ch = '*';
		bots[i].info.hp = MAX_HP;
		bots[i].info.pos_x = rand() % (WINDOW_SIZE - 2) + 1;
		bots[i].info.pos_y = rand() % (WINDOW_SIZE - 2) + 1;
		add_ball(game_win, &bots[i].info);
	}

	while (1)
	{
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			if (bots[i].id == 0)
				continue;

			// Get a random direction
			direction_t dir = random() % 4 + 1;
			handle_move(game_win, &bots[i], dir);
			board_grid[bots[i].info.pos_x][bots[i].info.pos_y] = bots[i].id;
		}
		wrefresh(game_win);
		sleep(3);
	}
}

// Thread function that handles the prizes
void *handle_prizes(void *arg)
{
	// Create initial prizes
	for (int i = 0; i < 5; i++)
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
		prizes[i].info.pos_x = x;
		prizes[i].info.pos_y = y;
		prizes[i].info.ch = value + '0';
		prizes[i].info.hp = value;
		prizes[i].id = -(i + 1);
		active_prizes++;
		add_ball(game_win, &prizes[i].info);
	}

	// Generate a new prize every 5 seconds
	while (1)
	{
		sleep(5);
		if (active_prizes == 10)
			continue;
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
		active_prizes++;
		add_ball(game_win, &prizes[active_prizes - 1].info);
		wrefresh(game_win);
	}
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int n_bots = 0;
	// Check arguments and its restrictions
	if (argc != 4)
	{
		printf("Usage: %s <server_IP> <server_port> <number_of_bots [1,10]>", argv[0]);
		exit(-1);
	}
	else if ((n_bots = atoi(argv[3])) < 1 || n_bots > 10)
	{
		printf("Bots number must be an integer in range [1,10]\n");
		exit(-1);
	}

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
	game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);

	// Create the message window
	msg_win = newwin(MAX_PLAYERS, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	// Store client information
	struct sockaddr_in client_address;
	socklen_t client_address_size;
	
	// Initialize global variables
	active_players = 0;
	active_prizes = 0;
	
	memset(active_chars, 0, sizeof(active_chars));

	memset(board_grid, 0, sizeof(board_grid));
	
	memset(players, 0, sizeof(players));
	memset(bots, 0, sizeof(bots));
	memset(prizes, 0, sizeof(prizes));

	// Create thread for handling bots
	pthread_t bots_thread;
	pthread_create(&bots_thread, NULL, handle_bots, &n_bots);

	// Create thread to handle prizes
	pthread_t prizes_thread;
	pthread_create(&prizes_thread, NULL, handle_prizes, NULL);


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
