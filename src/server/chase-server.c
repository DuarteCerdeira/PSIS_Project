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
#include <sys/un.h>

/* Local libraries */
#include "../chase.h"

#define INIT_X WINDOW_SIZE / 2 // Initial x position
#define INIT_Y WINDOW_SIZE / 2 // Initial y position

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

static int active_players;
static int active_prizes;
static char active_chars[MAX_PLAYERS];

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

struct client_info *handle_player_connection(WINDOW *win, struct sockaddr_un client)
{
	// Player limit reached
	if (active_players >= MAX_PLAYERS)
	{
		return NULL;
	}

	// Extract the client pid from the address
	char *client_address = client.sun_path;
	char *client_address_pid = strrchr(client_address, '-') + 1;

	// Select a random character to assign to the player
	char rand_char;
	srand(time(NULL));
	do
	{
		rand_char = rand() % ('Z' - 'A') + 'A';
	} while (strchr(active_chars, rand_char) != NULL);

	// Save player information
	players[active_players].id = atoi(client_address_pid);
	players[active_players].info.ch = rand_char;
	players[active_players].info.hp = MAX_HP;
	players[active_players].info.pos_x = INIT_X;
	players[active_players].info.pos_y = INIT_Y;

	// Add the assigned character to the list of
	// used characters
	active_chars[active_players] = rand_char;

	add_ball(win, &players[active_players].info);

	return &players[active_players++];
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

// Thread function that handles bots movement
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

int main(int argc, char *argv[])
{
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
	int server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (server_socket == -1)
	{
		perror("socket");
		exit(-1);
	}
	// Bind address
	struct sockaddr_un server_address;
	server_address.sun_family = AF_UNIX;
	sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

	unlink(server_address.sun_path);

	int err = bind(server_socket, (struct sockaddr *)&server_address,
				   sizeof(server_address));
	if (err == -1)
	{
		perror("bind");
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
	struct sockaddr_un client_address;
	socklen_t client_address_size;

	active_players = 0;					 // was not initialized, danger [A]
	memset(players, 0, sizeof(players)); // was not initialized, caused seggs fault [A]
	memset(bots, 0, sizeof(bots));		 // was not initialized

	// Create thread for handling bots movement
	pthread_t bots_thread;
	pthread_create(&bots_thread, NULL, handle_bots, &n_bots);

	while (1)
	{
		// Wait for messages from clients

		memset(&client_address.sun_path, '\0', sizeof(client_address.sun_path));
		client_address_size = sizeof(struct sockaddr_un);

		struct msg_data msg = {0};
		int nbytes = recvfrom(server_socket, &msg, sizeof(msg), 0,
							  (struct sockaddr *)&client_address, &client_address_size);

		// Error occurred
		if (nbytes == -1)
		{
			perror("recvfrom ");
			exit(-1);
		}

		// Client disconnected (EOF received)
		if (nbytes == 0)
		{
			long player_id = (long)atoi(strrchr(client_address.sun_path, '-') + 1);
			struct client_info *player = select_ball(player_id);
			handle_player_disconnection(game_win, player);
			continue;
		}

		switch (msg.type)
		{
		case (CONN):
		{
			msg = (struct msg_data){0}; // Cleaning msg so we can reuse it

			// Player trying to connect
			struct client_info *player = handle_player_connection(game_win, client_address);

			// Player limit reached: reject connection
			if (player == NULL)
			{
				msg.type = RJCT;
			}

			// Send ball info message
			else
			{
				// Setting up the ball info msg
				// We'll send the ball info by writing it
				// in the array that usually holds the field status
				msg.type = BINFO;
				msg.player_id = player->id;
				msg.field[0].ch = player->info.ch;
				msg.field[0].pos_x = player->info.pos_x;
				msg.field[0].pos_y = player->info.pos_y;
				msg.field[0].hp = player->info.hp;
				msg.dir = NONE;
			}
			break;
		}
		case (DCONN):
		{
			// Player trying to disconnect
			struct client_info *player = select_ball(msg.player_id);

			// Player not found
			if (player == NULL)
			{
				continue;
			}

			handle_player_disconnection(game_win, player);
			break;
		}
		case (BMOV):
		{
			// Ball can be a Bot or a Player
			struct client_info *player = select_ball(msg.player_id);
			direction_t dir = msg.dir;

			msg = (struct msg_data){0}; // Cleaning msg so we can reuse it

			// Ball not found: do nothing
			if (player == NULL)
			{
				continue;
			}

			// Ball HP is 0: disconnect player (only players lose health)
			if (player->info.hp == 0)
			{
				msg.type = HP0;
				handle_player_disconnection(game_win, player);
			}
			else
			{
				handle_move(game_win, player, dir);

				// Send field status message
				msg.type = FSTATUS;
				field_status(msg.field);
				update_stats(msg_win, msg.field);
				msg.player_id = player->id;
			}
			break;
		}
		default:
			continue;
		}

		// Send message back to client
		sendto(server_socket, &msg, sizeof(msg), 0,
			   (struct sockaddr *)&client_address, client_address_size);

		memset(&msg, 0, sizeof(msg));

		// Update message window
		print_player_stats(msg_win);

		wrefresh(msg_win);
		wrefresh(game_win);
	}
	endwin();
	exit(0);
}
