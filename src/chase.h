#include "../lib/board.h"

// Server Socket
#define SOCKET_PREFIX "/tmp/chase-socket"

// Maximum HP
#define MAX_HP 10
// Maximum number of balls is the area available in the board
#define MAX_BALLS (WINDOW_SIZE * WINDOW_SIZE)
// Maximum number of prizes
#define MAX_PRIZES 10

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
	ball_info_t field[MAX_BALLS];
};
