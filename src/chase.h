// Server Socket
#define SOCKET_PREFIX "/tmp/chase-socket"

// Size of the game window
#define WINDOW_SIZE 20		   // Window size
#define INIT_X WINDOW_SIZE / 2 // Initial x position
#define INIT_Y WINDOW_SIZE / 2 // Initial y position
#define INIT_HP 5			   // Initial HP
#define MAX_HP 10			   // Maximum HP
#define MAX_PLAYERS 10		   // Maximum number of players

// Message types
typedef enum msg_type_t
{
	CONN,
	DCONN,
	BINFO,
	BMOV,
	FSTATUS,
	HP0,
	RJCT
} msg_type_t;

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

// Message data
struct msg_data
{
	int player_id;
	msg_type_t type;
	direction_t dir;
	int hp;
	char ch;
	player_info_t field[MAX_PLAYERS * 3]; // max size -> 10 players + 10 bots + 10 prizes
};
