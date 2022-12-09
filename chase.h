#define WINDOW_SIZE 20

typedef enum msg_type {con, discon, binf, bmov, fstat, hp0} msg_type;

typedef enum direction_t {UP, DOWN, LEFT, RIGHT} direction_t;

struct msg_info {
    msg_type type;
    direction_t dir;
    int player_id;
    char ch;
};
