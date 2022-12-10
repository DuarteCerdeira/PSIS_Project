/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */
#include <sys/socket.h>
#include <sys/un.h>

/* Local libraries */
#include "chase.h"

/* Global variables */
player_info_t players[10];
player_info_t bots[10];
int n_players;
char active_chars[10];

int handle_connection (struct sockaddr *client, struct msg_data msg)
{
    if (n_players >= 10) {
	return -1;
    }

    // extract the client pid from the address
    char client_address[108];
    strcpy(client_address, ((struct sockaddr_un *) client)->sun_path);
    char *client_address_pid = strrchr(client_address, '-') + 1;

    // select a random character to assign to the player
    char rand_char;
    do {
	 rand_char = rand()%('Z'-'A') + 'A';
    } while (strchr(active_chars, rand_char) != NULL);

    players[n_players].player_id = atoi(client_address_pid);
    players[n_players].ch = rand_char;
    players[n_players].hp = 10;
    players[n_players].pos_x = WINDOW_SIZE/2;
    players[n_players].pos_y = WINDOW_SIZE/2;

    return 0;
}

void handle_disconnection (struct msg_data msg)
{
    int id = msg.player_id;

    int i;
    for(i = 0; i < n_players; i++) {
	if (players[i].player_id == id) {
	    break;
	}
    }

    players[i].player_id = 0;
    players[i].ch = '\0';
    players[i].hp = 0;
    players[i].pos_x = 0;
    players[i].pos_y = 0;
}

int main()
{
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

    int err = bind(server_socket,
		   (struct sockaddr *) &server_address,
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
    socklen_t client_address_size;

    while (1) {
	// wait for messages from clients
	struct msg_data msg;
	int nbytes = recvfrom(server_socket, &msg, sizeof(msg), 0,
			      (struct sockaddr *) &client_address,
			      &client_address_size);

	if (nbytes == -1) {
	    perror("recvfrom");
	    exit(-1);
	}
	if (nbytes == 0) {
	    msg.type = DCONN;
	}

	switch(msg.type) {
	case(CONN):
	    if (handle_connection((struct sockaddr *) &client_address, msg) == -1) {
		msg.type = RJCT;
	    } else {
		msg.type = BINFO;
		msg.ch = players[n_players].ch;
		msg.player_id = players[n_players].player_id;
	    }
	    break;
	case(DCONN):
	    handle_disconnection(msg);
	    n_players--;
	    break;
	case(BMOV):
	    // TODO: handle movement
	default:
	    break;
	}

	sendto(server_socket, &msg, sizeof(msg), 0,
	       (struct sockaddr *) &client_address,
	       client_address_size);

	wrefresh(msg_win);
    }
    endwin();
    exit(0);
}
