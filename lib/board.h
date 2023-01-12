#include <ncurses.h>

#define WINDOW_SIZE 30		   // Window size

// Direction
typedef enum direction
{
	NONE,
	UP,
	DOWN,
	LEFT,
	RIGHT
} direction_t;

// Player information
typedef struct ball_info
{
	int pos_x;
	int pos_y;
	int hp;
	char ch;
} ball_info_t;

void move_ball(WINDOW *win, ball_info_t *player, direction_t dir);
void add_ball(WINDOW *win, ball_info_t *player);
void delete_ball(WINDOW *win, ball_info_t *player);

void update_field(WINDOW *win, ball_info_t players[]);
void update_stats(WINDOW *win, ball_info_t players[]);
