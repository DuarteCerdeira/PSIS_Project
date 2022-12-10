// Server Socket
#define SOCKET_PREFIX "/tmp/chase-socket"

// Size of the game window
#define WINDOW_SIZE 20

// Message types
typedef enum msg_type_t {CONN, DCONN, BINFO, BMOV, FSTATUS, HP0, RJCT} msg_type_t;

// Direction
typedef enum direction_t {UP, DOWN, LEFT, RIGHT} direction_t;

// Message data
struct msg_data {
    msg_type_t type;
    direction_t dir;
    int player_id;
    char ch;
};

// Player information
typedef struct player_info_t {
    int player_id;
    int pos_x;
    int pos_y;
    int hp;
    char ch;
} player_info_t;
