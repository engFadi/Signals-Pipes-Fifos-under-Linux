/* Project-wide constants */
#ifndef SIGNALS_PIPES_CONSTANTS_H
#define SIGNALS_PIPES_CONSTANTS_H

#define CONFIG_DEFAULT_PATH "user_input.txt"
#define MIN_CHILDREN 2
#define DEFAULT_FURNITURE_PIECES 1000

/* buffer sizes for config parsing */
#define LINE_BUF 256
#define KEY_BUF 128

/* random token range */
#define RANDOM_MIN 1
#define RANDOM_MAX 9

/* Furniture piece enum and struct */
typedef enum {
    AVAILABLE,
    MOVING_FORWARD,
    MOVING_BACKWARD,
    PLACED,
    BLOCKED
} piece_status;

typedef struct {
    int serial_no;
    int order;
    piece_status status;
} furniture_piece;

#endif /* SIGNALS_PIPES_CONSTANTS_H */
