#include <ncurses.h>

#define WINDOW_SIZE 20		   // Window size

// Direction
typedef enum direction_t
{
	NONE,
	UP,
	DOWN,
	LEFT,
	RIGHT
} direction_t;

// Player information
typedef struct player_info_t
{
	int pos_x;
	int pos_y;
	int hp;
	char ch;
} player_info_t;

void move_player(WINDOW *win, player_info_t *player, direction_t dir);
void delete_player(WINDOW *win, player_info_t *player);
void add_player(WINDOW *win, player_info_t *player);
