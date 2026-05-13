#ifndef SIGNALS_PIPES_FURNITURE_H
#define SIGNALS_PIPES_FURNITURE_H

#include "constants.h"


furniture_piece *furniture_init(int count, int auto_serial);


void furniture_free(furniture_piece *furniture);


void furniture_display_table(furniture_piece *furniture, int count, int round);


void furniture_reset_serials(furniture_piece *furniture, int count);

#endif 
