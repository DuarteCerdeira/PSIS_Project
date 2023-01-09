/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* Threads */
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
	int fd;
	enum client_type {PLAYER, BOT, PRIZE} type;
	ball_info_t info;
};

/* Global variables */
static char active_chars[MAX_PLAYERS];
static struct client_info balls[10 + 10 + MAX_PLAYERS];
static int active_balls;

// Windows
static WINDOW *game_win;
static WINDOW *msg_win;

static char board_grid[WINDOW_SIZE][WINDOW_SIZE] = {0};

void field_status(ball_info_t *field)
{
	// Fills out players information
	for (int i = 0; i < active_balls; i++)
	{
		field[i].ch = balls[i].info.ch;
		field[i].hp = balls[i].info.hp;
		field[i].pos_x = balls[i].info.pos_x;
		field[i].pos_y = balls[i].info.pos_y;
	}
	return;
}

void handle_player_disconnection(WINDOW *win, struct client_info *player)
{
	// Delete the ball from the board
	delete_ball(win, &player->info);
	board_grid[player->info.pos_x][player->info.pos_y] = 0;

	// Delete the character from the list of used characters
	*strrchr(active_chars, player->info.ch) = active_chars[active_balls - 1];
	active_chars[active_balls - 1] = '\0';

	// Delete player information
	*player = balls[active_balls - 1];
	memset(&balls[active_balls - 1], 0, sizeof(struct client_info));

	active_balls--;
}

void handle_move(WINDOW *win, struct client_info *ball, direction_t dir)
{
	int ball_id;

	// Check if the position the ball wants to move to is clear
	switch (dir)
	{
	case UP:
		ball_id = board_grid[ball->info.pos_x][ball->info.pos_y - 1];
		break;
	case DOWN:
		ball_id = board_grid[ball->info.pos_x][ball->info.pos_y + 1];
		break;
	case LEFT:
		ball_id = board_grid[ball->info.pos_x - 1][ball->info.pos_y];
		break;
	case RIGHT:
		ball_id = board_grid[ball->info.pos_x + 1][ball->info.pos_y];
		break;
	default:
		break;
	}

	// No ball was hit
	if (ball_id == -1)
	{
		// Ball position is updated
		/* Critical Region */
		board_grid[ball->info.pos_x][ball->info.pos_y] = 0;
		move_ball(win, &ball->info, dir);
		board_grid[ball->info.pos_x][ball->info.pos_y] = ball->info.ch;
		/* =============== */
		return;
	}

	/* Critical Region */
	struct client_info ball_hit = balls[ball_id];
	/* =============== */
	
	// Collision happened - in most cases, the ball doesn't move
	direction_t new_dir = NONE;

	// Player hit a prize
	if (ball_hit.type == PRIZE && ball->type != BOT)
	{
		// Player's health is updated
		int health = ball_hit.info.hp;
		ball->info.hp += (ball->info.hp + health > MAX_HP) ? MAX_HP - ball->info.hp : health;

		// Prize is deleted
		
		/* Critical Region */
		struct client_info last_ball = balls[active_balls - 1];

		board_grid[last_ball.info.pos_x][last_ball.info.pos_y] = ball_id;
		ball_hit = last_ball;
		memset(&balls[active_balls - 1], 0, sizeof(struct client_info));
		active_balls--;
		/* =============== */
		
		// Player position needs to be updated
		new_dir = dir;
	}

	// Ball (player or bot) hit a player
	else if (ball_hit.type != BOT)
	{
		// Ball "steals" 1 HP from the player
		ball->info.hp == MAX_HP ? MAX_HP : ball->info.hp++;
		
		/* Critical Region */
		balls[ball_id].info.hp == 0 ? 0 : balls[ball_id].info.hp--;
		/* =============== */
	}

	/* Critical Region */
	board_grid[ball->info.pos_x][ball->info.pos_y] = 0;
	move_ball(win, &ball->info, new_dir);
	board_grid[ball->info.pos_x][ball->info.pos_y] = ball->info.ch;
	/* =============== */
}

void print_player_stats(WINDOW *win)
{
	struct client_info balls_copy[10 + 10 + MAX_PLAYERS];
	/* Critical Region */
	memcpy(balls_copy, balls, sizeof(balls));
	/* =============== */
	
	ball_info_t p_stats[MAX_PLAYERS] = {0};
	for (size_t i = 0; i < active_balls; i++)
	{
		if (balls_copy[i].type != PLAYER)
			continue;

		p_stats[i].ch = balls_copy[i].info.ch;
		p_stats[i].hp = balls_copy[i].info.hp;
		p_stats[i].pos_x = balls_copy[i].info.pos_x;
		p_stats[i].pos_y = balls_copy[i].info.pos_y;
	}

	update_stats(win, p_stats);
	return;
}

// Thread function that handles bots
void *handle_bots(void *arg)
{
	int n_bots = *(int *)arg;
	struct client_info new_bots[n_bots];

	// Create bots
	for (int i = 0; i < n_bots; i++)
	{
		// Initialize bots information
		new_bots[i].type = BOT;
		new_bots[i].info.ch = '*';
		new_bots[i].info.hp = MAX_HP;
		new_bots[i].info.pos_x = rand() % (WINDOW_SIZE - 2) + 1;
		new_bots[i].info.pos_y = rand() % (WINDOW_SIZE - 2) + 1;
		add_ball(game_win, &new_bots[i].info);
	}

	/* Critical Region */
	memcpy(balls, new_bots, sizeof(new_bots));
	active_balls += n_bots;
	/* =============== */

	while (1)
	{
		for (int i = 0; i < n_bots; i++)
		{
			// Get a random direction
			direction_t dir = random() % 4 + 1;
			handle_move(game_win, &balls[i], dir);
			board_grid[balls[i].info.pos_x][balls[i].info.pos_y] = balls[i].info.ch;
		}
		wrefresh(game_win);
		sleep(3);
	}
}

// Thread function that handles the prizes
void *handle_prizes(void *arg)
{
	//struct client_info new_prizes[10];
	
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
		} while (board_grid[x][y] != -1);

		// Generate a prize with a random value between 1 and 5
		int value = rand() % 5 + 1;
		/* Critical Region */
	    balls[active_balls].info.pos_x = x;
		balls[active_balls].info.pos_y = y;
		balls[active_balls].info.ch = value + '0';
		balls[active_balls].info.hp = value;
		balls[active_balls].type = PRIZE;
		add_ball(game_win, &balls[i].info);
		active_balls++;
		/* =============== */
		wrefresh(game_win);
	}

	// Generate a new prize every 5 seconds
	while (1)
	{
		sleep(5);
		if (active_balls == 10)
			continue;
		int x;
		int y;

		// Generate a random position that is not occupied
		do
		{
			x = rand() % (WINDOW_SIZE - 2) + 1;
			y = rand() % (WINDOW_SIZE - 2) + 1;
		} while (board_grid[x][y] != -1);

		// Generate a prize with a random value between 1 and 5
		int value = rand() % 5 + 1;
		/* Critical Region */
		balls[active_balls].info.pos_x = x;
		balls[active_balls].info.pos_y = y;
		balls[active_balls].info.ch = value + '0';
		balls[active_balls].info.hp = value;
		balls[active_balls].type = PRIZE;
		active_balls++;
		/* =============== */
		
		add_ball(game_win, &balls[active_balls - 1].info);
		wrefresh(game_win);
	}
}

ball_info_t create_ball()
{
	ball_info_t new_ball;
	
	// Select a random character to assign to the player
	char rand_char;
	srand(time(NULL));

	/* == Critical Region == */
	do
	{
		rand_char = rand() % ('Z' - 'A') + 'A';
	} while (strchr(active_chars, rand_char) != NULL);

	active_chars[active_balls] = rand_char;
	/* ===================== */

	// Save player information
    new_ball.ch = rand_char;
	new_ball.hp = MAX_HP;
	new_ball.pos_x = INIT_X;
	new_ball.pos_y = INIT_Y;

	add_ball(game_win, &new_ball);

	return new_ball;
}

void *client_thread (void *arg)
{
	struct client_info client;

	client.fd = *(int *) arg;

	while(client.fd != -1) {
		struct msg_data msg = {0};
		int nbytes;
		char buffer[sizeof(struct msg_data)];

		// Receive message from client
		nbytes = 0;
		memset(buffer, 0, sizeof(struct msg_data));
		
		do {
			char *ptr = &buffer[nbytes];
			nbytes += recv(client.fd, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data));

		memcpy(&msg, buffer, sizeof(buffer));

		switch (msg.type) {
		case (CONN):
			if (active_balls >= MAX_PLAYERS) {
				close(client.fd);
				client.fd = -1;
				continue;
			}
			client.info = create_ball();
			client.type = PLAYER;

			/* == Critical Region == */
			balls[active_balls++] = client;
			/* ===================== */
			
			memset(&msg, 0, sizeof(struct msg_data));
			
			msg.type = BINFO;
			msg.field[0] = client.info;

			break;
		case (DCONN):
			break;
		case (BMOV):
			if (client.info.hp == 0) {
				msg.type = HP0;
			}

			handle_move(game_win, &client, msg.dir);

			memset(&msg, 0, sizeof(struct msg_data));
			
			msg.type = FSTATUS;
			field_status(msg.field);
			// TODO: update stats
			// TODO: send field status to everyone
			
			break;
		default:
			continue;
		}

		// Send message to client
		nbytes = 0;
		memset(buffer, 0, sizeof(struct msg_data));

		memcpy(buffer, &msg, sizeof(struct msg_data));
		
		do {
			char *ptr = &buffer[nbytes];
			nbytes += send(client.fd, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data));

	}
	
	return NULL;
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
	active_balls = 0;
	active_balls = 0;
	
	memset(active_chars, 0, sizeof(active_chars));

	memset(board_grid, -1, sizeof(board_grid));
	
	memset(balls, 0, sizeof(balls));
	memset(balls, 0, sizeof(balls));
	memset(balls, 0, sizeof(balls));

	// Create thread for handling bots
	pthread_t bots_thread;
	pthread_create(&bots_thread, NULL, handle_bots, &n_bots);

	// Create thread to handle prizes
	pthread_t prizes_thread;
	pthread_create(&prizes_thread, NULL, handle_prizes, NULL);

	pthread_t client_threads[MAX_PLAYERS];
	int n_clients = 0;


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

		if (active_balls >= MAX_PLAYERS) {
			close(c_fd);
			continue;
		}

		/* Create threads for each client */

		int *thread_arg = malloc(sizeof(int));
		*thread_arg = c_fd;
		
		pthread_create(&client_threads[n_clients++], NULL, client_thread, thread_arg);
		// pthread_join(thread_id[--active_players], NULL);
	}
}
