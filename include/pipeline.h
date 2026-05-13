#ifndef SIGNALS_PIPES_PIPELINE_H
#define SIGNALS_PIPES_PIPELINE_H

#include "constants.h"
#include "visual.h"

int run_pipeline(int child_count,
				 furniture_piece *furniture,
				 int furniture_count,
				 double min_pause,
				 double max_pause,
				 int team_id,
                 VisualShm *visual);

#endif 
