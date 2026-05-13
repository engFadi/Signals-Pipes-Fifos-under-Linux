#ifndef SIGNALS_PIPES_PIPELINE_SHARED_H
#define SIGNALS_PIPES_PIPELINE_SHARED_H

#include "constants.h"

furniture_piece *pipeline_map_shared_furniture(furniture_piece *source, int count);
void pipeline_unmap_shared_furniture(furniture_piece *shared, furniture_piece *original, int count);
int pipeline_find_piece_index(furniture_piece *furniture, int count, int serial);

#endif 
