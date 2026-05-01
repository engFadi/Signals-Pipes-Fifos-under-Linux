#ifndef SIGNALS_PIPES_PIPELINE_ROLES_H
#define SIGNALS_PIPES_PIPELINE_ROLES_H

#include "pipeline_internal.h"

void run_source(PipelineContext *ctx, int forward_fd[2], int backward_fd[2]);
void run_sink(PipelineContext *ctx, int index, int forward_fd[2], int backward_fd[2]);
void run_middle(PipelineContext *ctx,
                int index,
                int forward_in[2],
                int forward_out[2],
                int backward_in[2],
                int backward_out[2]);

#endif /* SIGNALS_PIPES_PIPELINE_ROLES_H */
