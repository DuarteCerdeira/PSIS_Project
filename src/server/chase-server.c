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
struct client_info {
	int id;
	player_info_t info;
};

/* Global variables */
static struct client_info players[10];
// static player_info_t bots[10];
static int active_players;
static char active_chars[10];

void draw(WINDOW *win, player_info_t player, bool delete) {
	if (delete) {
		mvwaddch(win, player.pos_y, player.pos_x, ' ');
	} else {
		mvwaddch(win, player.pos_y, player.pos_x, player.ch);
	}
}

void move_player(WINDOW *win, player_info_t *player, direction_t dir) {
	draw(win, *player, true);

	int new_x = player->pos_x;
	int new_y = player->pos_y;

	switch (dir) {
	case UP:
		if (player->pos_y > 0) {
			new_y--;
		}
		break;
	case DOWN:
		if (player->pos_y < WINDOW_SIZE - 1) {
			new_y++;
		}
		break;
	case LEFT:
		if (player->pos_x > 0) {
			new_x--;
		}
		break;
	case RIGHT:
		if (player->pos_x < WINDOW_SIZE - 1) {
			new_x++;
		}
		break;
	default:
		break;
	}

	player->pos_x = new_x;
	player->pos_y = new_y;
	draw(win, *player, false);
}

struct client_info *check_collision(int x, int y) {
	int i;
	while (i < MAX_PLAYERS || (players[i].info.pos_x != x && players[i].info.pos_y != y)) {
		i++;
	}

	return i == MAX_PLAYERS ? NULL : &players[i];
}

struct client_info *select_player(int id) {
	int i;
	while (i < MAX_PLAYERS || players[i].id != id) {
		i++;
	}

	return i == MAX_PLAYERS ? NULL : &players[i];
}

struct client_info *handle_connection(WINDOW *win, struct sockaddr_un client) {
	if (active_players >= 10) {
		return NULL;
	}

	// extract the client pid from the address
	char *client_address = client.sun_path;
	char *client_address_pid = strrchr(client_address, '-') + 1;

	// select a random character to assign to the player
	char rand_char;
	do {
		rand_char = rand() % ('Z' - 'A') + 'A';
	} while (strchr(active_chars, rand_char) != NULL);

	int free_spot = 0;
	while (players[free_spot].id != -1) {
		free_spot++;
	}

	players[free_spot].id = atoi(client_address_pid);
	players[free_spot].info.ch = rand_char;
	players[free_spot].info.hp = INIT_HP;
	players[free_spot].info.pos_x = INIT_X;
	players[free_spot].info.pos_y = INIT_Y;

	return &players[free_spot];
}

void handle_disconnection(WINDOW *win, struct client_info *player) {
	player->id = -1;
}

void handle_move(WINDOW *win, struct client_info *player, direction_t dir) {
	struct client_info *player_hit;

	switch (dir) {
	case UP:
		player_hit = check_collision(player->info.pos_x, player->info.pos_y - 1);
		break;
	case DOWN:
		player_hit = check_collision(player->info.pos_x, player->info.pos_y + 1);
		break;
	case LEFT:
		player_hit = check_collision(player->info.pos_x - 1, player->info.pos_y);
		break;
	case RIGHT:
		player_hit = check_collision(player->info.pos_x + 1, player->info.pos_y);
		break;
	default:
		break;
	}

	if (player_hit) {
		player->info.hp == MAX_HP ? MAX_HP : player->info.hp++;
		player_hit->info.hp == 0 ? 0 : player_hit->info.hp--;
		dir = NONE;
	}

	move_player(win, &player->info, dir);
}

int main() {
	// open socket
	int server_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (server_socket == -1) {
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
	if (err == -1) {
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
	WINDOW *msg_win = newwin(10, WINDOW_SIZE, WINDOW_SIZE, 0);
	box(msg_win, 0, 0);
	wrefresh(msg_win);

	// store client information
	struct sockaddr_un client_address;
	socklen_t client_address_size = sizeof(client_address);

	memset(&client_address.sun_path, '\0', sizeof(client_address.sun_path));

	while (1) {
		// wait for messages from clients
		struct msg_data msg;
		int nbytes =
			recvfrom(server_socket, &msg, sizeof(msg), 0,
					 (struct sockaddr *)&client_address, &client_address_size);

		if (nbytes == -1) {
			perror("recvfrom");
			exit(-1);
		}
		if (nbytes == 0) {
			int player_id = atoi(strrchr(client_address.sun_path, '-') + 1);
			struct client_info *player = select_player(player_id);
			handle_disconnection(game_win, player);
			continue;
		}

		switch (msg.type) {
		case (CONN): {
			struct client_info *player = handle_connection(game_win, client_address);
			if (player == NULL) {
				// player limit reached: reject connection
				msg.type = RJCT;
			} else {
				msg.type = BINFO;
				msg.player_id = player->id;
				msg.ch = player->info.ch;
			}
			break;
		}
		case (DCONN): {
			struct client_info *player = select_player(msg.player_id);
			if (player == NULL) {
				// player not found: do nothing
				continue;
			}

			handle_disconnection(game_win, player);
			active_players--;
			break;
		}
		case (BMOV): {
			struct client_info *player = select_player(msg.player_id);
			if (player == NULL) {
				// player not found: do nothing
				continue;
			}
			if (player->info.hp == 0) {
				// player hp is 0: disconnect player
				handle_disconnection(game_win, player);
				msg.type = HP0;
				active_players--;
			} else {
				handle_move(game_win, player, msg.dir);
				msg.type = FSTATUS;
			}
			break;
		}
		default:
			continue;
		}

		sendto(server_socket, &msg, sizeof(msg), 0,
			   (struct sockaddr *)&client_address, client_address_size);
		
		memset(&msg, 0, sizeof(msg));
		
		wrefresh(msg_win);
	}
	endwin();
	exit(0);
}
