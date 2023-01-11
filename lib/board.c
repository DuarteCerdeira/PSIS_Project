#include "board.h"

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

void add_ball(WINDOW *win, ball_info_t *ball) { draw(win, *ball, false); }

void delete_ball(WINDOW *win, ball_info_t *ball) { draw(win, *ball, true); }

void update_field(WINDOW *win, ball_info_t players[])
{
	werase(win);
	box(win, 0, 0);
	int i = 0;
	while (players[i].ch != 0)
	{
		mvwaddch(win, players[i].pos_y, players[i].pos_x, players[i].ch);
		i++;
	}
}

void update_stats(WINDOW *win, ball_info_t players[])
{
	werase(win);
	box(win, 0, 0);
	int line = 0;
	for (int i = 0; players[i].ch != 0; i++)
	{
		if (players[i].ch > 'A' && players[i].ch < 'Z')
		{
			mvwprintw(win, line + 1, 1, "%c %d", players[i].ch, players[i].hp);
			line++;
		}
	}
}
