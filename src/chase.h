#include "../lib/board.h"

// Server Socket
#define SOCKET_PREFIX "/tmp/chase-socket"

// Maximum HP
#define MAX_HP 10
// Maximum number of players: squares avaliable in the board - border squares - max prizes (10) - max bots (10)
#define MAX_PLAYERS (WINDOW_SIZE * WINDOW_SIZE - 2 * WINDOW_SIZE - 2 * (WINDOW_SIZE - 2) - 2 * 10)

// Message types
typedef enum msg_type
{
	CONN,
	BINFO,
	BMOV,
	FSTATUS,
	HP0,
	CONTGAME
} msg_type_t;

// Message data
struct msg_data
{
	msg_type_t type;
	direction_t dir;
	ball_info_t field[MAX_PLAYERS];
};
