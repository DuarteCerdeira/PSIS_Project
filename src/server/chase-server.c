/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

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

// Error handling function
extern int errno;

/* Client information structure */
struct client_info
{
	int fd;
	enum client_type
	{
		EMPTY,
		PLAYER,
		BOT,
		PRIZE
	} type;
	ball_info_t info;
};

/* Args sent by a client thread to a respawn timer thread it creates */
struct thread_args
{
	int client_index;
	pthread_t client_thread;
};

/* Global variables */

// Global so we can orderly close the socket on CTRL + C
int server_socket;

// Array to store the ball information
static struct client_info balls[MAX_BALLS];

// Matrix to store the board state
static int board_grid[WINDOW_SIZE][WINDOW_SIZE] = {0};

// These variables are to limit access to the free spaces stack
static pthread_mutex_t mux_free_spaces = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_free_spaces = PTHREAD_COND_INITIALIZER;

// Tracks the number of prizes on the board
static int n_prizes;
static pthread_mutex_t mux_n_prizes = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_n_prizes = PTHREAD_COND_INITIALIZER;

// Conditional variable used to broadcast field info
static pthread_cond_t cond_field_status = PTHREAD_COND_INITIALIZER;

// The two main critical regions are the access to ball positions and health
static pthread_mutex_t mux_health = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mux_position = PTHREAD_MUTEX_INITIALIZER;

// Windows
static WINDOW *game_win;
static WINDOW *stats_win;
static pthread_mutex_t mux_stats_win = PTHREAD_MUTEX_INITIALIZER;

// Function to deal with CTRL + C as an orderly shutdown
void sigint_handler(int signum)
{
	for (int i = 0; i < MAX_BALLS; i++)
	{
		if (balls[i].type == PLAYER)
			close(balls[i].fd);
	}
	close(server_socket);
	endwin();
	exit(0);
}

void field_status(ball_info_t *field)
{
	// Fills out player information
	int f_index = 0;
	for (int i = 0; i < MAX_BALLS; i++)
	{
		if (balls[i].type == EMPTY)
			continue;
		field[f_index].ch = balls[i].info.ch;
		field[f_index].hp = balls[i].info.hp;
		field[f_index].pos_x = balls[i].info.pos_x;
		field[f_index].pos_y = balls[i].info.pos_y;
		f_index++;
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

void delete_player(int index)
{
	/* Critical region position start */
	pthread_mutex_lock(&mux_position);

	/* Critical region health start */
	pthread_mutex_lock(&mux_health);
	
	// Delete the player from the board
	delete_ball(game_win, &balls[index].info);
	board_grid[balls[index].info.pos_x][balls[index].info.pos_y] = -1;
	
	pthread_mutex_unlock(&mux_position);
	/* Critical region position end */

	// Delete player information
	close(balls[index].fd);
	memset(&balls[index], 0, sizeof(struct client_info));

	/* Critical region free spaces start */
	pthread_mutex_lock(&mux_free_spaces);

	stack_push(index);
	pthread_cond_signal(&cond_free_spaces);
	
	pthread_mutex_unlock(&mux_free_spaces);
	/* Critical region free spaces end */

	pthread_mutex_unlock(&mux_health);
	/* Critical region health end */

	pthread_cond_signal(&cond_field_status);
}

void handle_move(int ball_id, direction_t dir, ball_info_t *local_ball)
{
	// Check if the position the ball wants to move to is clear
	int ball_hit_id;
	int x = local_ball->pos_x, y = local_ball->pos_y;
	int new_x, new_y;
	switch (dir)
	{
	case UP:
		if (y < 1)
		{
			return;
		}
		new_x = x;
		new_y = y - 1;
		
		/* Critical Region position start */
		pthread_mutex_lock(&mux_position);
		
		ball_hit_id = board_grid[new_x][new_y];
		break;
	case DOWN:
		if (y > WINDOW_SIZE - 1)
		{
			return;
		}
		new_x = x;
		new_y = y + 1;
		
		/* Critical Region position start */
		pthread_mutex_lock(&mux_position);
		
		ball_hit_id = board_grid[new_x][new_y];
		break;
	case LEFT:
		if (x < 1)
		{
			return;
		}
		new_x = x - 1;
		new_y = y;
		
		/* Critical Region position start */
		pthread_mutex_lock(&mux_position);
		
		ball_hit_id = board_grid[new_x][new_y];
		break;
	case RIGHT:
		if (x > WINDOW_SIZE - 1)
		{
			return;
		}
		new_x = x + 1;
		new_y = y;
		
		/* Critical Region position start */
		pthread_mutex_lock(&mux_position);
		
		ball_hit_id = board_grid[new_x][new_y];
		break;
	default:
		break;
	}

	// No ball was hit
	if (ball_hit_id == -1)
	{
		// Ball position is updated
		board_grid[x][y] = -1;
		board_grid[new_x][new_y] = ball_id;

		move_ball(game_win, &balls[ball_id].info, dir);

		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */

		local_ball->pos_x = new_x;
		local_ball->pos_y = new_y;
		
		return;
	}

	/* Critical region health start */
	pthread_mutex_lock(&mux_health);
	
	struct client_info ball = balls[ball_id];
	struct client_info ball_hit = balls[ball_hit_id];

	// Player hit a prize
	if (ball_hit.type == PRIZE && ball.type == PLAYER)
	{
		board_grid[x][y] = -1;
		board_grid[new_x][new_y] = ball_id;

		// Player's health is updated
		int prize_hp = ball_hit.info.hp;
		int new_hp = ball.info.hp;

		new_hp += (new_hp + prize_hp > MAX_HP) ? MAX_HP - new_hp : prize_hp;
	    
		balls[ball_id].info.hp = ball.info.hp;
		balls[ball_id].info.pos_x = new_x;
		balls[ball_id].info.pos_y = new_y;

		memset(&balls[ball_hit_id], 0, sizeof(struct client_info));

		/* Critical region free_spaces start */
		pthread_mutex_lock(&mux_free_spaces);

		stack_push(ball_hit_id);
		pthread_cond_signal(&cond_free_spaces);
		
		pthread_mutex_unlock(&mux_free_spaces);
		/* Critical region free_spaces end */

		/* Critical region n_prizes start */
		pthread_mutex_lock(&mux_n_prizes);
		
		n_prizes--;
		pthread_cond_signal(&cond_n_prizes);
		
		pthread_mutex_unlock(&mux_n_prizes);
		/* Critical region n_prizes end */
		
		pthread_mutex_unlock(&mux_health);
		/* Critical region health end */
		
		move_ball(game_win, &ball.info, dir);
		
		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */

		local_ball->hp = new_hp;
		local_ball->pos_x = new_x;
		local_ball->pos_y = new_y;
		
		
		return;
	}

	// Ball (player or bot) hit a player
	if (ball_hit.type == PLAYER)
	{
		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */

		int ball_hit_hp = ball_hit.info.hp;
		int ball_hp = ball.info.hp;

		if (ball_hit_hp > 0) {
			ball_hp += (ball_hp == MAX_HP) ? 0 : 1;
			ball_hit_hp -= 1;
		}

		// Ball "steals" 1 HP from the player
		balls[ball_id].info.hp = ball_hp;
		balls[ball_hit_id].info.hp = ball_hit_hp;

		pthread_mutex_unlock(&mux_health);
		/* Critical region health end */

		local_ball->hp = ball_hp;
		
		return;
	}

	pthread_mutex_unlock(&mux_health);
	/* Critical region health end */
	
	pthread_mutex_unlock(&mux_position);
	/* Critical region position end */

}

void print_player_stats()
{
	struct client_info balls_copy[10 + 10 + MAX_BALLS];
	
	/* Critical region health start*/
	pthread_mutex_lock(&mux_health);
	
	memcpy(balls_copy, balls, sizeof(balls));
	
	pthread_mutex_unlock(&mux_health);
	/* Critical region health end */

	ball_info_t p_stats[MAX_BALLS] = {0};
	for (size_t i = 0; i < MAX_BALLS; i++)
	{
		if (balls_copy[i].type != PLAYER)
			continue;

		p_stats[i].ch = balls_copy[i].info.ch;
		p_stats[i].hp = balls_copy[i].info.hp;
		p_stats[i].pos_x = balls_copy[i].info.pos_x;
		p_stats[i].pos_y = balls_copy[i].info.pos_y;
	}

	/* Critical region stats window start */
	pthread_mutex_lock(&mux_stats_win);
	
	update_stats(stats_win, p_stats);
	
	pthread_mutex_unlock(&mux_stats_win);
	/* Critical region stats window end */
	
	return;
}

// Thread function that handles bots
void *handle_bots(void *arg)
{
	int n_bots = *(int *)arg;
	int bot_index[n_bots];
	struct client_info bots[n_bots];

	// Create bots
	for (int i = 0; i < n_bots; i++)
	{
		int x;
		int y;

		/* Critical region position start */
		pthread_mutex_lock(&mux_position);

		/* Critical region health start */
		pthread_mutex_lock(&mux_health);
		
		do
		{
			x = rand() % (WINDOW_SIZE - 2) + 1;
			y = rand() % (WINDOW_SIZE - 2) + 1;
		} while (board_grid[x][y] != -1);

		// Initialize bots information
		bots[i].type = BOT;
		bots[i].info.ch = '*';
		bots[i].info.hp = MAX_HP;
		bots[i].info.pos_x = x;
		bots[i].info.pos_y = y;

		/* Critical region free_spaces start */
		pthread_mutex_lock(&mux_free_spaces);
		
		bot_index[i] = stack_pop();
		
		pthread_mutex_unlock(&mux_free_spaces);
		/* Critical region free_spaces end */
		
		memcpy(&balls[bot_index[i]], &bots[i], sizeof(bots[i]));
		
		board_grid[bots[i].info.pos_x][bots[i].info.pos_y] = bot_index[i];

		pthread_mutex_unlock(&mux_health);
		/* Critical region health end */

		add_ball(game_win, &bots[i].info);
		
		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */
	}

	while (1)
	{
		sleep(3);
		for (int i = 0; i < n_bots; i++)
		{
			// Get a random direction
			direction_t dir = random() % 4 + 1;
			handle_move(bot_index[i], dir, &(bots[i].info));
		}
		pthread_cond_signal(&cond_field_status);
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
		/* Critical region n_prizes start */
		pthread_mutex_lock(&mux_n_prizes);
		
		// do nothing if the board is full or there are already
		// the maximum number of prizes
		if (n_prizes == MAX_PRIZES)
		{
		    pthread_cond_wait(&cond_n_prizes, &mux_n_prizes);
		}
		
		pthread_mutex_unlock(&mux_n_prizes);
		/* Critical region n_prizes end */

		/* Critical region free_spaces start */
		pthread_mutex_lock(&mux_free_spaces);

		if (stack_is_empty()) {
			pthread_cond_wait(&cond_free_spaces, &mux_free_spaces);
		}
		
		int index = stack_pop();
		
		pthread_mutex_unlock(&mux_free_spaces);
		/* Critical region free_spaces end */

		// Generate the first five prizes
		if (first_prizes < 5)
		{
			first_prizes++;
		}
		else
		{
			sleep(5);
		}

		/* Critical region position start */
		pthread_mutex_lock(&mux_position);
		
		/* Critical region health start */
		pthread_mutex_lock(&mux_health);

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

		memcpy(&balls[index], &new_prize, sizeof(struct client_info));
		
		board_grid[x][y] = index;

		/* Critical region n_prizes start */
		pthread_mutex_lock(&mux_n_prizes);
		n_prizes++;
		pthread_mutex_unlock(&mux_n_prizes);
		/* Critical region n_prizes end */
		
		add_ball(game_win, &new_prize.info);

		pthread_mutex_unlock(&mux_health);
		/* Critical region health end */
		
		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */

		pthread_cond_signal(&cond_field_status);
	}
}

// Thread function to wait 10s for a dead player's continue game
void *respawn_timer(void *args)
{
	// This thread will be killed if the client sends a msg
	struct thread_args params = *(struct thread_args *)args;
	int index = params.client_index;
	pthread_t c_thread = params.client_thread;

	sleep(10);
	// If not, just delete the ball and disconnect the player
	delete_player(index);
	pthread_cancel(c_thread);
	return NULL;
}

// Thread function that handles each client
void *client_thread(void *arg)
{
	struct client_info client = {0};
	int index = 0;
	int nbytes = 0;
	struct msg_data msg;
	char buffer[sizeof(struct msg_data)];
	pthread_t respawn_thread; // Respawn timer thread variable in case we need it
	pthread_t self_thread = pthread_self();

	client.fd = *(int *)arg;

	while (client.fd != -1)
	{
		msg = (struct msg_data){0};

		// Receive message from client
		memset(buffer, 0, sizeof(buffer));
		nbytes = 0;

		// Check if the client sent any message after dying (to reconnect)
		if (client.info.ch != 0 && client.info.hp == 0)
		{
			// Send this thread's ID to the respawn timer thread in case it needs to shut it down
			struct thread_args args = {index, self_thread};
			// Create thread to countdown the respawn time
			pthread_create(&respawn_thread, NULL, respawn_timer, &args);
		}

		// Receive message from client
		do
		{
			nbytes = 0;
			char *ptr = &buffer[nbytes];
			nbytes += recv(client.fd, ptr, sizeof(buffer) - nbytes, 0);
		} while (nbytes < sizeof(struct msg_data) && nbytes > 0);

		// Socket was closed, either by the client or by the respawn timeout
		if (nbytes <= 0)
			break;

		// Stop the respawn timer thread if the client sent a message
		if (client.info.ch != 0 && client.info.hp == 0)
			pthread_cancel(respawn_thread);

		memcpy(&msg, buffer, sizeof(buffer));

		switch (msg.type)
		{
		case (CONN):
			
			/* Critical region free_spaces start */
			pthread_mutex_lock(&mux_free_spaces);
			
			// If the board is full reject the player
			if (stack_is_empty())
			{
				close(client.fd);
				client.fd = -1;

				pthread_mutex_unlock(&mux_free_spaces);
				/* Critical region free_spaces end */
				
				return NULL;
			}

			index = stack_pop();

			pthread_mutex_unlock(&mux_free_spaces);
			/* Critical region free_spaces end */

			/* Critical region position start */
			pthread_mutex_lock(&mux_position);

			// Create a new ball structure
			client.info = create_ball();
			client.type = PLAYER;

			/* Critical region health start */
			pthread_mutex_lock(&mux_health);

			// Update balls structure with new player
			balls[index] = client;

			pthread_mutex_unlock(&mux_health);
			/* Critical region health end */
			
			// Update the board
			board_grid[client.info.pos_x][client.info.pos_y] = index;
			add_ball(game_win, &client.info);
			
			pthread_mutex_unlock(&mux_position);
			/* Critical region position end */

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
			// Health may have changed in the meantime
			/* Critical region health start */
			pthread_mutex_lock(&mux_health);

			client.info.hp = balls[index].info.hp;

			pthread_mutex_unlock(&mux_health);
			/* Critical region health end */

			if (client.info.hp == 0)
			{
				// Player is dead
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
				handle_move(index, msg.dir, &client.info);
			}

			break;
		case (CONTGAME):
			// Revive the player
			/* Critical region health start */
			pthread_mutex_lock(&mux_health);
			
			balls[index].info.hp = MAX_HP;
			client = balls[index];

			pthread_mutex_unlock(&mux_health);
			/* Critical region health end */
			break;
		default:
			continue;
		}

		pthread_cond_signal(&cond_field_status);
	}

	delete_player(index);

	return NULL;
}

// Thread function to broadcast the field status to all clients
void *field_update(void *arg)
{
	while(1) {
		/* Critical region position start */
		pthread_mutex_lock(&mux_position);
		pthread_cond_wait(&cond_field_status, &mux_position);
		
		/* Critical region health start */
		pthread_mutex_lock(&mux_health);

		struct msg_data msg = {0};

		msg.type = FSTATUS;
		field_status(msg.field);

		for (int i = 0; i < MAX_BALLS; i++)
		{
			// Send to (active) clients only
			if (balls[i].type != PLAYER)
				continue;

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
		
		pthread_mutex_unlock(&mux_health);
		/* Critical region health end */

		pthread_mutex_unlock(&mux_position);
		/* Critical region position end */
	}
}

int main(int argc, char *argv[])
{
	// We want to catch CTRL + C
	signal(SIGINT, sigint_handler);
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
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
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
	memset(board_grid, -1, sizeof(board_grid));
	memset(balls, 0, sizeof(balls));

	// Initialize free spaces stack
	stack_init(MAX_BALLS);
	for(int i = MAX_BALLS - 1; i >= 0; i--) {
		stack_push(i);
	}

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

		int c_fd = -1;

		// This loop is to handle the case when the accept() call is interrupted by a signal
		// Which seems to be very frequent
		do
		{
			c_fd = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
		} while (c_fd == -1 && errno == EINTR);

		/* Create threads for each client */

		int *thread_arg = malloc(sizeof(int));
		*thread_arg = c_fd;

		pthread_create(&client_threads[n_clients++], NULL, client_thread, thread_arg);
	}
}
