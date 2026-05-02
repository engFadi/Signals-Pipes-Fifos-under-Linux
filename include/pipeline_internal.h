#ifndef SIGNALS_PIPES_PIPELINE_INTERNAL_H
#define SIGNALS_PIPES_PIPELINE_INTERNAL_H

#include <sys/types.h>
#include "constants.h"

typedef struct {
    furniture_piece *furniture;
    int furniture_count;
    int expected_order;
    double current_min_pause;
    double current_max_pause;
    pid_t source_pid;
} PipelineContext;

#endif /* SIGNALS_PIPES_PIPELINE_INTERNAL_H */
