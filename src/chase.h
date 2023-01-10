#include "../lib/board.h"

// Server Socket
#define SOCKET_PREFIX "/tmp/chase-socket"

#define MAX_HP 10	   // Maximum HP
#define MAX_PLAYERS 10 // Maximum number of players, bots AND prizes

// Message types
typedef enum msg_type
{
	CONN,
	BINFO,
	BMOV,
	FSTATUS,
	HP0,
} msg_type_t;

// Message data
struct msg_data
{
	msg_type_t type;
	direction_t dir;
	ball_info_t field[MAX_PLAYERS * 3]; // max size -> 10 players + 10 bots + 10 prizes
};
