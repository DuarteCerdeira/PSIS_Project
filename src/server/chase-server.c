/* Standard libraries */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Threads */
#include <pthread.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

/* Local libraries */
#include "../chase.h"

#define INIT_X WINDOW_SIZE / 2 // Initial x position
#define INIT_Y WINDOW_SIZE / 2 // Initial y position

/* Client information structure */
struct client_info
{
	int fd;
	enum client_type
	{
		PLAYER,
		BOT,
		PRIZE
	} type;
	ball_info_t info;
};

/* Global variables */

// Each global variable has a corresponding mutex
static struct client_info balls[MAX_BALLS];
static pthread_mutex_t mux_balls;

static int active_balls;
static pthread_mutex_t mux_active_balls;

static int n_prizes;
static pthread_mutex_t mux_n_prizes;

// Conditional variable used to broadcast field info
static pthread_cond_t cond_field_status = PTHREAD_COND_INITIALIZER;

// Windows
static WINDOW *game_win;
pthread_mutex_t mux_game_win;
static WINDOW *stats_win;
pthread_mutex_t mux_stats_win;

static char board_grid[WINDOW_SIZE][WINDOW_SIZE] = {0};
static pthread_mutex_t mux_board_grid;

void field_status(ball_info_t *field)
{
	struct client_info balls_copy[MAX_BALLS];
	memcpy(balls_copy, balls, sizeof(balls));
	int act = active_balls;
	/* =============== */

	// Fills out player information
	for (int i = 0; i < act; i++)
	{
		field[i].ch = balls_copy[i].info.ch;
		field[i].hp = balls_copy[i].info.hp;
		field[i].pos_x = balls_copy[i].info.pos_x;
		field[i].pos_y = balls_copy[i].info.pos_y;
	}

	pthread_mutex_lock(&mux_stats_win);
	update_stats(stats_win, field);
	wrefresh(stats_win);
	pthread_mutex_unlock(&mux_stats_win);

	return;
}

ball_info_t create_ball()
{
	ball_info_t new_ball;

	// Select a random character to assign to the player
	char rand_char;
	srand(time(NULL));

	rand_char = rand() % ('Z' - 'A') + 'A';

	// Save player information
	new_ball.ch = rand_char;
	new_ball.hp = MAX_HP;
	// Generate a random position that is not occupied
	do
	{
		new_ball.pos_x = rand() % (WINDOW_SIZE - 2) + 1;
		new_ball.pos_y = rand() % (WINDOW_SIZE - 2) + 1;
	} while (board_grid[new_ball.pos_x][new_ball.pos_y] != -1);

	return new_ball;
}

void handle_move(int ball_id, direction_t dir)
{
	struct client_info ball = balls[ball_id];

	// Check if the position the ball wants to move to is clear
	int ball_hit_id;
	switch (dir)
	{
	case UP:
		if (ball.info.pos_y < 1)
		{
			return;
		}
		ball_hit_id = board_grid[ball.info.pos_x][ball.info.pos_y - 1];
		break;
	case DOWN:
		if (ball.info.pos_y > WINDOW_SIZE - 1)
		{
			return;
		}
		ball_hit_id = board_grid[ball.info.pos_x][ball.info.pos_y + 1];
		break;
	case LEFT:
		if (ball.info.pos_x < 1)
		{
			return;
		}
		ball_hit_id = board_grid[ball.info.pos_x - 1][ball.info.pos_y];
		break;
	case RIGHT:
		if (ball.info.pos_x > WINDOW_SIZE - 1)
		{
			return;
		}
		ball_hit_id = board_grid[ball.info.pos_x + 1][ball.info.pos_y];
		break;
	default:
		break;
	}

	// No ball was hit
	if (ball_hit_id == -1)
	{
		// Ball position is updated

		board_grid[ball.info.pos_x][ball.info.pos_y] = -1;
		move_ball(game_win, &ball.info, dir);
		board_grid[ball.info.pos_x][ball.info.pos_y] = ball_id;

		memcpy(&balls[ball_id], &ball, sizeof(struct client_info));
		return;
	}

	struct client_info ball_hit = balls[ball_hit_id];

	// Collision happened - in most cases, the ball doesn't move
	direction_t new_dir = NONE;

	// Player hit a prize
	if (ball_hit.type == PRIZE && ball.type != BOT)
	{
		// Player's health is updated
		int health = ball_hit.info.hp;
		ball.info.hp += (ball.info.hp + health > MAX_HP) ? MAX_HP - ball.info.hp : health;

		// Prize is deleted
		struct client_info last_ball = balls[active_balls - 1];

		board_grid[last_ball.info.pos_x][last_ball.info.pos_y] = ball_hit_id;
		ball_hit = last_ball;

		memset(&balls[active_balls - 1], 0, sizeof(struct client_info));

		pthread_mutex_lock(&mux_n_prizes);
		n_prizes--;
		pthread_mutex_unlock(&mux_n_prizes);

		active_balls--;

		// Player position needs to be updated
		new_dir = dir;
	}

	// Ball (player or bot) hit a player
	else if (ball_hit.type != BOT)
	{
		// Ball "steals" 1 HP from the player
		ball.info.hp == MAX_HP ? MAX_HP : ball.info.hp++;
		ball_hit.info.hp == 0 ? 0 : ball_hit.info.hp--;
	}

	board_grid[ball.info.pos_x][ball.info.pos_y] = -1;
	move_ball(game_win, &ball.info, new_dir);
	board_grid[ball.info.pos_x][ball.info.pos_y] = ball_id;

	memcpy(&balls[ball_id], &ball, sizeof(struct client_info));
	memcpy(&balls[ball_hit_id], &ball_hit, sizeof(struct client_info));
}

// Thread function that handles bots
void *handle_bots(void *arg)
{
	int n_bots = *(int *)arg;
	int bot_index[n_bots];
	struct client_info new_bot;

	// Create bots
	for (int i = 0; i < n_bots; i++)
	{
		int x;
		int y;

		/* Critical Region */
		pthread_mutex_lock(&mux_active_balls);
		pthread_mutex_lock(&mux_board_grid);
		pthread_mutex_lock(&mux_balls);
		do
		{
			x = rand() % (WINDOW_SIZE - 2) + 1;
			y = rand() % (WINDOW_SIZE - 2) + 1;
		} while (board_grid[x][y] != -1);

		// Initialize bots information
		new_bot.type = BOT;
		new_bot.info.ch = '*';
		new_bot.info.hp = MAX_HP;
		new_bot.info.pos_x = x;
		new_bot.info.pos_y = y;

		memcpy(&balls[active_balls], &new_bot, sizeof(new_bot));
		bot_index[i] = active_balls;
		board_grid[new_bot.info.pos_x][new_bot.info.pos_y] = active_balls;
		active_balls++;

		pthread_mutex_unlock(&mux_balls);
		pthread_mutex_unlock(&mux_board_grid);
		pthread_mutex_unlock(&mux_active_balls);
		/* =============== */

		pthread_mutex_lock(&mux_game_win);
		add_ball(game_win, &new_bot.info);
		wrefresh(game_win);
		pthread_mutex_unlock(&mux_game_win);
	}

	while (1)
	{
		for (int i = 0; i < n_bots; i++)
		{
			int index = bot_index[i];
			// Get a random direction
			direction_t dir = random() % 4 + 1;
			/* Critical Region */
			pthread_mutex_lock(&mux_board_grid);
			pthread_mutex_lock(&mux_active_balls);
			pthread_mutex_lock(&mux_balls);
			pthread_mutex_lock(&mux_game_win);
			handle_move(index, dir);
			wrefresh(game_win);
			pthread_mutex_unlock(&mux_game_win);
			pthread_mutex_unlock(&mux_balls);
			pthread_mutex_unlock(&mux_active_balls);
			pthread_mutex_unlock(&mux_board_grid);
			/* =============== */
		}
		pthread_cond_signal(&cond_field_status);
		sleep(3);
	}
}

// Thread function that handles the prizes
void *handle_prizes(void *arg)
{
	struct client_info new_prize;
	int first_prizes = 0;

	// Generate a new prize every 5 seconds
	while (1)
	{
		// Generate the first five prizes
		if (first_prizes < 5)
		{
			first_prizes++;
		}
		else
		{
			sleep(5);
		}

		/* Critical Region */
		pthread_mutex_lock(&mux_active_balls);
		pthread_mutex_lock(&mux_board_grid);
		pthread_mutex_lock(&mux_balls);

		// do nothing if the board is full or there are already
		// the maximum number of prizes
		if (n_prizes == MAX_PRIZES || active_balls >= MAX_BALLS)
		{
			pthread_mutex_unlock(&mux_balls);
			pthread_mutex_unlock(&mux_board_grid);
			pthread_mutex_unlock(&mux_active_balls);
			continue;
		}

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

		new_prize.info.pos_x = x;
		new_prize.info.pos_y = y;
		new_prize.info.ch = value + '0';
		new_prize.info.hp = value;
		new_prize.type = PRIZE;

		memcpy(&balls[active_balls], &new_prize, sizeof(struct client_info));

		board_grid[x][y] = active_balls;

		active_balls++;

		pthread_mutex_unlock(&mux_balls);
		pthread_mutex_unlock(&mux_board_grid);
		pthread_mutex_unlock(&mux_active_balls);
		/* =============== */

		pthread_mutex_lock(&mux_n_prizes);
		n_prizes++;
		pthread_mutex_unlock(&mux_n_prizes);

		pthread_mutex_lock(&mux_game_win);
		add_ball(game_win, &new_prize.info);
		wrefresh(game_win);
		pthread_mutex_unlock(&mux_game_win);

		pthread_cond_signal(&cond_field_status);
	}
}

// Thread function that handles each client
void *client_thread(void *arg)
{
	struct client_info client;
	int index;
	int nbytes = 0;
	struct msg_data msg;
	char buffer[sizeof(struct msg_data)];

	client.fd = *(int *)arg;

	while (client.fd != -1)
	{
		nbytes = 0;
		msg = (struct msg_data){0};
		memset(buffer, 0, sizeof(buffer));

		// Receive message from client
		if (client.info.ch != 0 && client.info.hp == 0)
		{
			sleep(10);
			ioctl(client.fd, FIONREAD, &nbytes);
			if (nbytes == 0)
				break;
		}
		do
		{
			nbytes = 0;
			char *ptr = &buffer[nbytes];
			nbytes += recv(client.fd, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data) && nbytes > 0);

		if (nbytes == 0)
			break;

		memcpy(&msg, buffer, sizeof(buffer));

		switch (msg.type)
		{
		case (CONN):

			/* Critical Region */
			pthread_mutex_lock(&mux_active_balls);
			pthread_mutex_lock(&mux_balls);
			pthread_mutex_lock(&mux_board_grid);

			// Create a new ball structure
			client.info = create_ball();
			client.type = PLAYER;

			// If the board is full reject the player
			if (active_balls == MAX_BALLS)
			{
				close(client.fd);
				client.fd = -1;

				pthread_mutex_unlock(&mux_board_grid);
				pthread_mutex_unlock(&mux_active_balls);
				pthread_mutex_unlock(&mux_balls);
				continue;
			}

			// Save the position of the player
			index = active_balls;

			// Update balls structure and game board with new player
			balls[active_balls++] = client;
			board_grid[client.info.pos_x][client.info.pos_y] = index;

			pthread_mutex_unlock(&mux_board_grid);
			pthread_mutex_unlock(&mux_active_balls);
			pthread_mutex_unlock(&mux_balls);
			/* =============== */

			// Update the board
			/* Critical Region */
			pthread_mutex_lock(&mux_game_win);
			add_ball(game_win, &client.info);
			wrefresh(game_win);
			pthread_mutex_unlock(&mux_game_win);
			/* =============== */

			// Send BINFO message back to the client
			memset(&msg, 0, sizeof(struct msg_data));

			msg.type = BINFO;
			msg.field[0] = client.info;

			nbytes = 0;
			memset(buffer, 0, sizeof(struct msg_data));

			// Copy the data to a byte buffer so there are no problems with
			// byte order
			memcpy(buffer, &msg, sizeof(struct msg_data));

			// This guarantees that all the data is sent
			do
			{
				char *ptr = &buffer[nbytes];
				nbytes += send(client.fd, ptr, sizeof(buffer) - nbytes, MSG_NOSIGNAL);
			} while (nbytes < sizeof(struct msg_data));
			break;

		case (BMOV):

			/* Critical Region */
			pthread_mutex_lock(&mux_active_balls);
			pthread_mutex_lock(&mux_board_grid);
			pthread_mutex_lock(&mux_balls);

			if (balls[index].info.hp == 0)
			{
				client = balls[index];
				memset(&msg, 0, sizeof(struct msg_data));
				msg.type = HP0;

				nbytes = 0;
				memset(buffer, 0, sizeof(struct msg_data));

				// Copy the data to a byte buffer so there are no problems with
				// byte order
				memcpy(buffer, &msg, sizeof(struct msg_data));

				// This guarantees that all the data is sent
				do
				{
					char *ptr = &buffer[nbytes];
					nbytes += send(client.fd, ptr, sizeof(buffer) - nbytes, MSG_NOSIGNAL);
				} while (nbytes < sizeof(struct msg_data));
			}
			else
			{
				pthread_mutex_lock(&mux_game_win);

				handle_move(index, msg.dir);
				wrefresh(game_win);

				// Update the client info in this thread after the move
				client = balls[index];

				pthread_mutex_unlock(&mux_game_win);
			}

			pthread_mutex_unlock(&mux_active_balls);
			pthread_mutex_unlock(&mux_board_grid);
			pthread_mutex_unlock(&mux_balls);

			break;
		case (CONTGAME):
			pthread_mutex_lock(&mux_balls);
			pthread_mutex_lock(&mux_active_balls);

			/* Critical Region */
			balls[index].info.hp = MAX_HP;
			client = balls[index];
			/* =============== */

			pthread_mutex_unlock(&mux_active_balls);
			pthread_mutex_unlock(&mux_balls);
			break;
		default:
			continue;
		}

		pthread_cond_signal(&cond_field_status);
	}

	// Delete the player
	pthread_mutex_lock(&mux_game_win);
	delete_ball(game_win, &client.info);
	wrefresh(game_win);
	pthread_mutex_unlock(&mux_game_win);

	/* == Critical Region == */
	pthread_mutex_lock(&mux_board_grid);
	board_grid[client.info.pos_x][client.info.pos_y] = -1;
	pthread_mutex_unlock(&mux_board_grid);
	/* ===================== */

	// Delete player information
	/* == Critical Region == */
	pthread_mutex_lock(&mux_balls);
	pthread_mutex_lock(&mux_active_balls);

	balls[index] = balls[active_balls - 1];
	memset(&balls[active_balls - 1], 0, sizeof(struct client_info));
	active_balls--;

	pthread_mutex_unlock(&mux_balls);
	pthread_mutex_unlock(&mux_active_balls);
	/* ===================== */
	close(client.fd);

	pthread_cond_signal(&cond_field_status);

	return NULL;
}

void *field_update(void *arg)
{
	while (1)
	{
		pthread_mutex_lock(&mux_balls);
		pthread_cond_wait(&cond_field_status, &mux_balls);

		struct msg_data msg = {0};

		msg.type = FSTATUS;
		field_status(msg.field);

		for (int i = 0; i < MAX_BALLS; i++)
		{
			if (balls[i].type != PLAYER)
			{
				continue;
			}
			// Send message to client
			int nbytes = 0;
			char buffer[sizeof(struct msg_data)] = {0};

			memcpy(buffer, &msg, sizeof(struct msg_data));

			do
			{
				char *ptr = &buffer[nbytes];
				nbytes += send(balls[i].fd, ptr, sizeof(buffer) - nbytes, MSG_NOSIGNAL);
			} while (nbytes < sizeof(struct msg_data));
		}

		pthread_mutex_unlock(&mux_balls);
	}
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int n_bots = 0;
	int sock_port = 0;
	// Check arguments and its restrictions
	if (argc != 4)
	{
		printf("Usage: %s <server_IP> <server_port> <number_of_bots [1,10]>", argv[0]);
		exit(-1);
	}
	else if (inet_addr(argv[1]) == INADDR_NONE)
	{
		printf("Invalid IP address\n");
		exit(-1);
	}
	else if ((sock_port = atoi(argv[2])) < 1024 || sock_port > 65535)
	{
		printf("Invalid server port\n");
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
		perror("socket: ");
		exit(-1);
	}

	// Bind address
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	if ((server_address.sin_port = htons(sock_port)) == 0)
	{
		perror("htons port: ");
		exit(-1);
	}
	if (inet_pton(AF_INET, argv[1], &server_address.sin_addr) <= 0)
	{
		perror("inet_pton: ");
		exit(-1);
	}
	if (bind(server_socket, (struct sockaddr *)&server_address,
			 sizeof(server_address)) == -1)
	{
		perror("bind: ");
		exit(-1);
	}
	if (listen(server_socket, 10) == -1)
	{
		perror("listen: ");
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
	stats_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, WINDOW_SIZE + 2); // TODO: CHANGE LATER
	box(stats_win, 0, 0);
	wrefresh(stats_win);

	// Store client information
	struct sockaddr_in client_address;
	socklen_t client_address_size;

	// Initialize global variables
	active_balls = 0;
	memset(board_grid, -1, sizeof(board_grid));
	memset(balls, 0, sizeof(balls));

	// Create thread for handling bots
	pthread_t bots_thread;
	pthread_create(&bots_thread, NULL, handle_bots, &n_bots);

	// Create thread to handle prizes
	pthread_t prizes_thread;
	pthread_create(&prizes_thread, NULL, handle_prizes, NULL);

	// Create thread to broadcast field info
	pthread_t field_update_thread;
	pthread_create(&field_update_thread, NULL, field_update, NULL);

	pthread_t client_threads[MAX_BALLS];
	int n_clients = 0;

	while (1)
	{
		// Wait for connection requests from clients
		memset(&client_address, 0, sizeof(client_address));
		client_address_size = sizeof(struct sockaddr_in);

		int c_fd = accept(server_socket, (struct sockaddr *)&client_address,
						  &client_address_size);

		if (c_fd == -1)
		{
			perror("accept");
			exit(-1);
		}

		/* Create threads for each client */

		int *thread_arg = malloc(sizeof(int));
		*thread_arg = c_fd;

		pthread_create(&client_threads[n_clients++], NULL, client_thread, thread_arg);
		// pthread_join(thread_id[--active_players], NULL);
	}
}
