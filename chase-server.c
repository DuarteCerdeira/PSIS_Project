/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>

/* NCurses */
#include <ncurses.h>

/* System libraries */

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

int main() {
    // open socket
    int server_sock;

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
    while (key != 'q') {
	key = wgetch(game_win);
	wrefresh(game_win);
    }
    endwin();
    exit(0);
}
