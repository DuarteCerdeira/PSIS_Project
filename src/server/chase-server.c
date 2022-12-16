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
#include <sys/un.h>

/* Local libraries */
#include "../chase.h"

/* Client information structure */
struct client_info
{
	int id;
	player_info_t info;
};

/* Global variables */
static struct client_info players[MAX_PLAYERS];
static struct client_info bots[MAX_PLAYERS];
static int active_players;
static char active_chars[MAX_PLAYERS];

struct client_info *check_collision(int x, int y, int id)
{
	int i = 0;

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (players[i].info.pos_x == x && players[i].info.pos_y == y && players[i].id != id)
		{
			return &players[i];
		}
		else if (bots[i].info.pos_x == x && bots[i].info.pos_y == y && bots[i].id != id)
		{
			return &bots[i];
		}
		// TODO: ADD PRIZES COLLISION STUFF
	}

	return NULL;
}

struct client_info *select_player(int id)
{
	int i;
	if (id >= BOTS_ID)
	{
		// The "player" is a bot
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (bots[i].id == id)
				break;
		}
		return i == MAX_PLAYERS ? NULL : &bots[i];
	}
	else
	{
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (players[i].id == id)
				break;
		}

		return i == MAX_PLAYERS ? NULL : &players[i];
	}
}

struct client_info *handle_connection(WINDOW *win, struct sockaddr_un client)
{
	// active_players was not initialized anywhere [A]
	if (active_players >= MAX_PLAYERS)
	{
		return NULL;
	}

	// extract the client pid from the address
	char *client_address = client.sun_path;
	char *client_address_pid = strrchr(client_address, '-') + 1;

	// select a random character to assign to the player
	char rand_char;
	srand(time(NULL));
	do
	{
		rand_char = rand() % ('Z' - 'A') + 'A';
	} while (strchr(active_chars, rand_char) != NULL);

	int free_spot = 0;
	while (players[free_spot].id != 0)
	{
		free_spot++;
	}

	players[free_spot].id = atoi(client_address_pid);
	players[free_spot].info.ch = rand_char;
	players[free_spot].info.hp = INIT_HP;
	players[free_spot].info.pos_x = INIT_X;
	players[free_spot].info.pos_y = INIT_Y;

	active_chars[free_spot] = rand_char;

	add_player(win, &players[free_spot].info);

	return &players[free_spot];
}

void field_status(player_info_t *field)
{
	int j = 0;
	for (int i = 0; (i < MAX_PLAYERS) && (players[i].id != 0); i++, j++)
	{
		field[i].ch = players[i].info.ch;
		field[i].hp = players[i].info.hp;
		field[i].pos_x = players[i].info.pos_x;
		field[i].pos_y = players[i].info.pos_y;
	}
	for (int i = 0; (i < MAX_PLAYERS) && (bots[i].id != 0); i++, j++)
	{
		field[j].ch = bots[i].info.ch;
		field[j].hp = bots[i].info.hp;
		field[j].pos_x = bots[i].info.pos_x;
		field[j].pos_y = bots[i].info.pos_y;
	}

	// for (i; i < MAX_PLAYERS + i; i++)
	// {
	// 	if (prizes[i] != NULL)
	// 	{
	// 		field[i].ch = itoa(prizes[i].value);
	// 		field[i].hp = -1;
	// 		field[i].pos_x = prizes[i].pos_x;
	// 		field[i].pos_y = prizes[i].pos_y;
	// 	}
	// }
	return;
}

void handle_disconnection(WINDOW *win, struct client_info *player)
{
	delete_player(win, &player->info);

	if (player->id < BOTS_ID)
		*strrchr(active_chars, player->info.ch) = '\0';

	memset(player, 0, sizeof(struct client_info));
}

void handle_move(WINDOW *win, struct client_info *player, direction_t dir)
{
	struct client_info *player_hit;

	switch (dir)
	{
	case UP:
		player_hit = check_collision(player->info.pos_x, player->info.pos_y - 1, player->id);
		break;
	case DOWN:
		player_hit = check_collision(player->info.pos_x, player->info.pos_y + 1, player->id);
		break;
	case LEFT:
		player_hit = check_collision(player->info.pos_x - 1, player->info.pos_y, player->id);
		break;
	case RIGHT:
		player_hit = check_collision(player->info.pos_x + 1, player->info.pos_y, player->id);
		break;
	default:
		break;
	}

	if (player_hit != NULL)
	{
		player->info.hp == MAX_HP ? MAX_HP : player->info.hp++;
		player_hit->info.hp == 0 ? 0 : player_hit->info.hp--;
		dir = NONE;
	}

	move_player(win, &player->info, dir);
}

void handle_bots_conn(WINDOW *win, struct player_info_t *bots_init_info)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (bots_init_info[i].ch == '\0')
			break;
		bots[i].id = BOTS_ID + i;
		bots[i].info.ch = bots_init_info[i].ch;
		bots[i].info.hp = bots_init_info[i].hp;
		bots[i].info.pos_x = bots_init_info[i].pos_x;
		bots[i].info.pos_y = bots_init_info[i].pos_y;
		add_player(win, &bots[i].info);
	}
	return;
}

int main()
{
	// open socket
	int server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (server_socket == -1)
	{
		perror("socket");
		exit(-1);
	}

	// bind address
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

	// ncurses initialization
	initscr();
	cbreak();
	noecho();

	// create the game window
	WINDOW *game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
	box(game_win, 0, 0);
	wrefresh(game_win);

	// create the message window
	WINDOW *msg_win = newwin(20, WINDOW_SIZE, 0, WINDOW_SIZE + 2);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	// store client information
	struct sockaddr_un client_address;
	socklen_t client_address_size = sizeof(client_address);
	memset(&client_address.sun_path, '\0', sizeof(client_address.sun_path));

	active_players = 0;					 // was not initialized, danger [A]
	memset(players, 0, sizeof(players)); // was not initialized, caused seggs fault [A]
	memset(bots, 0, sizeof(bots));		 // was not initialized

	while (1)
	{
		// wait for messages from clients
		struct msg_data msg = {0};
		int nbytes = recvfrom(server_socket, &msg, sizeof(msg), 0,
							  (struct sockaddr *)&client_address, &client_address_size);

		if (nbytes == -1)
		{
			perror("recvfrom ");
			exit(-1);
		}
		if (nbytes == 0)
		{
			int player_id = atoi(strrchr(client_address.sun_path, '-') + 1);
			struct client_info *player = select_player(player_id);
			handle_disconnection(game_win, player);
			continue;
		}

		switch (msg.type)
		{
		case (CONN):
		{
			// (I left this if like this because i don't like nested IFs) [A]
			// special case for bots client connection
			if (msg.player_id == BOTS_ID - 1 && bots[0].id != 0)
			{
				// bots already connected
				// this prevents bots client from connecting more than once
				continue;
			}
			else if (msg.player_id == BOTS_ID - 1)
			{
				// bots connection
				handle_bots_conn(game_win, msg.field);
				break;
			}
			struct client_info *player = handle_connection(game_win, client_address);
			msg = (struct msg_data){0};
			if (player == NULL)
			{
				// player limit reached: reject connection
				msg.type = RJCT;
			}
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
			struct client_info *player = select_player(msg.player_id);
			if (player == NULL)
			{
				// player not found: do nothing
				continue;
			}

			handle_disconnection(game_win, player);
			active_players--;

			break;
		}
		case (BMOV):
		{
			// "player" can be a bot or a client
			struct client_info *player = select_player(msg.player_id);
			direction_t dir = msg.dir;
			msg = (struct msg_data){0}; // Cleaning msg so we can reuse it
			if (player == NULL)
			{
				// player not found: do nothing
				continue;
			}
			if (player->info.hp == 0)
			{
				// player hp is 0: disconnect player
				if (player->id < BOTS_ID)
				{
					// is a client, not a bot
					active_players--;
					msg.type = HP0;
				}
				handle_disconnection(game_win, player);
			}
			else
			{
				handle_move(game_win, player, dir);

				if (player->id < BOTS_ID)
				{
					// is a client, not a bot
					msg.type = FSTATUS;
					field_status(msg.field);
					msg.player_id = player->id;
				}
			}
			break;
		}
		default:
			continue;
		}
		// This sendto is still okay when the sender was the bot client
		// because the client address is empty
		// So msg will be sent to no one
		sendto(server_socket, &msg, sizeof(msg), 0,
			   (struct sockaddr *)&client_address, client_address_size);

		memset(&msg, 0, sizeof(msg));

		wrefresh(msg_win);
		wrefresh(game_win);
	}
	endwin();
	exit(0);
}
