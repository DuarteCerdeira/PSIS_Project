#include "../lib/board.h"

void draw(WINDOW *win, player_info_t player, bool delete)
{
	if (delete)
	{
		mvwaddch(win, player.pos_y, player.pos_x, ' ');
	}
	else
	{
		mvwaddch(win, player.pos_y, player.pos_x, player.ch);
	}
}

void move_player(WINDOW *win, player_info_t *player, direction_t dir)
{
	draw(win, *player, true);

	int new_x = player->pos_x;
	int new_y = player->pos_y;

	switch (dir)
	{
	case UP:
		if (player->pos_y > 1)
		{
			new_y--;
		}
		break;
	case DOWN:
		if (player->pos_y < WINDOW_SIZE - 2)
		{
			new_y++;
		}
		break;
	case LEFT:
		if (player->pos_x > 1)
		{
			new_x--;
		}
		break;
	case RIGHT:
		if (player->pos_x < WINDOW_SIZE - 2)
		{
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

void delete_player(WINDOW *win, player_info_t *player)
{
	draw(win, *player, true);
}

void add_player(WINDOW *win, player_info_t *player)
{
	draw(win, *player, false);
}
