#ifndef SIGNALS_PIPES_PIPELINE_INTERNAL_H
#define SIGNALS_PIPES_PIPELINE_INTERNAL_H

#include <sys/types.h>
#include "constants.h"
#include "visual.h"

typedef enum {
    PIPE_EVENT_PICKED,
    PIPE_EVENT_FORWARD,
    PIPE_EVENT_BACKWARD,
    PIPE_EVENT_PLACED,
    PIPE_EVENT_REJECTED,
    PIPE_EVENT_BLOCKED,
    PIPE_EVENT_RELEASED
} PipelineEventType;

typedef struct {
    furniture_piece *furniture;
    int furniture_count;
    int expected_order;
    double current_min_pause;
    double current_max_pause;
    pid_t source_pid;
    int team_id;
    VisualShm *visual;
} PipelineContext;

#endif 
