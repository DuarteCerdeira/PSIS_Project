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

/* Custom libraries */
#include "chase.h"

/* Structures */
typedef struct player_info_t {
    int player_id;
    int pos_x;
    int pos_y;
    int hp;
    char ch;
} player_info_t;

/* Global variables */
player_info_t players[10];
player_info_t bots[10];

int main()
{
    // open socket
    int server_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server_sock == -1) {
	perror("socket");
	exit(-1);
    }

    // bind address    
    struct sockaddr_un server_address;
    server_address.sun_family = AF_UNIX;
    sprintf(server_address.sun_path, "%s-%s", SOCKET_PREFIX, "server");

    unlink(server_address.sun_path);

    int err = bind(server_sock,
		   (struct sockaddr *) &server_address,
		   sizeof(server_address));
    if (err == -1) {
	perror("bind");
	exit(-1);
    }

    // ncurses initialization
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    // create the game window
    WINDOW *game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(game_win, 0, 0);
    wrefresh(game_win);

    // create the message window
    WINDOW *msg_win = newwin(10, WINDOW_SIZE, WINDOW_SIZE, 0);
    box(msg_win, 0, 0);
    wrefresh(msg_win);

    int key = -1;
    char msg;
    while (key != 'q') {
	key = wgetch(game_win);
	mvwprintw(game_win, WINDOW_SIZE/2, WINDOW_SIZE/2 - 4, "Pressed %c", key);
	wrefresh(game_win);
	
	recv(server_sock, &msg, sizeof(msg), 0);
	mvwprintw(msg_win, 1, 1, "Received %c", msg);
	wrefresh(msg_win);
    }
    endwin();
    exit(0);
}
