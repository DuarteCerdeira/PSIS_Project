#include "../lib/board.h"

void draw(WINDOW *win, ball_info_t ball, bool delete)
{
	if (delete)
	{
		mvwaddch(win, ball.pos_y, ball.pos_x, ' ');
	}
	else
	{
		mvwaddch(win, ball.pos_y, ball.pos_x, ball.ch);
	}
}

void move_ball(WINDOW *win, ball_info_t *ball, direction_t dir)
{
	draw(win, *ball, true);

	int new_x = ball->pos_x;
	int new_y = ball->pos_y;

	switch (dir)
	{
	case UP:
		if (ball->pos_y > 1)
		{
			new_y--;
		}
		break;
	case DOWN:
		if (ball->pos_y < WINDOW_SIZE - 2)
		{
			new_y++;
		}
		break;
	case LEFT:
		if (ball->pos_x > 1)
		{
			new_x--;
		}
		break;
	case RIGHT:
		if (ball->pos_x < WINDOW_SIZE - 2)
		{
			new_x++;
		}
		break;
	default:
		break;
	}

	ball->pos_x = new_x;
	ball->pos_y = new_y;
	draw(win, *ball, false);
}

void delete_ball(WINDOW *win, ball_info_t *ball)
{
	draw(win, *ball, true);
}

void add_ball(WINDOW *win, ball_info_t *ball)
{
	draw(win, *ball, false);
}
