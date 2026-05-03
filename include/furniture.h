#ifndef SIGNALS_PIPES_FURNITURE_H
#define SIGNALS_PIPES_FURNITURE_H

#include "constants.h"

/* Initialize furniture array with serial numbers and status */
furniture_piece *furniture_init(int count, int auto_serial);

/* Free furniture array */
void furniture_free(furniture_piece *furniture);

/* Display furniture table */
void furniture_display_table(furniture_piece *furniture, int count, int round);

/* Reset furniture for a new round: all pieces become AVAILABLE with new shuffled serials */
void furniture_reset_serials(furniture_piece *furniture, int count);

#endif /* SIGNALS_PIPES_FURNITURE_H */
