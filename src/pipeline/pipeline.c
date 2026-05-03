#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "constants.h"
#include "pipeline.h"
#include "pipeline_internal.h"
#include "pipeline_io.h"
#include "pipeline_roles.h"
#include "pipeline_shared.h"

int run_pipeline(int child_count,
                 furniture_piece *furniture,
                 int furniture_count,
                 double min_pause,
                 double max_pause,
                 int team_id) {
    if (child_count < MIN_CHILDREN) {
        child_count = MIN_CHILDREN;
    }

    furniture_piece *shared_furniture = pipeline_map_shared_furniture(furniture, furniture_count);
    if (shared_furniture == NULL) {
        return EXIT_FAILURE;
    }

    PipelineContext ctx;
    ctx.furniture = shared_furniture;
    ctx.furniture_count = furniture_count;
    ctx.expected_order = 0;
    ctx.current_min_pause = min_pause;
    ctx.current_max_pause = max_pause;
    ctx.source_pid = 0;
    ctx.team_id = team_id;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    int (*sv)[2] = malloc((size_t)(child_count - 1) * sizeof(*sv));
    if (sv == NULL) {
        perror("malloc");
        pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
        return EXIT_FAILURE;
    }

    int (*rv)[2] = malloc((size_t)(child_count - 1) * sizeof(*rv));
    if (rv == NULL) {
        perror("malloc");
        free(sv);
        pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < child_count - 1; ++i) {
        if (pipe(sv[i]) == -1) {
            perror("pipe (forward)");
            for (int j = 0; j < i; ++j) {
                close_pipe_pair(sv[j]);
                close_pipe_pair(rv[j]);
            }
            free(sv);
            free(rv);
            pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < child_count - 1; ++i) {
        if (pipe(rv[i]) == -1) {
            perror("pipe (backward)");
            for (int j = 0; j < child_count - 1; ++j) {
                close_pipe_pair(sv[j]);
                close_pipe_pair(rv[j]);
            }
            free(sv);
            free(rv);
            pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < child_count; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            for (int j = 0; j < child_count - 1; ++j) {
                close_pipe_pair(sv[j]);
                close_pipe_pair(rv[j]);
            }
            free(sv);
            free(rv);
            pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
            return EXIT_FAILURE;
        }

        if (i == 0 && pid > 0) {
            ctx.source_pid = pid;
        }

        if (pid == 0) {
            for (int j = 0; j < child_count - 1; ++j) {
                if (j != i - 1) {
                    close(sv[j][0]);
                }
                if (j != i) {
                    close(sv[j][1]);
                }
                if (j != i - 1) {
                    close(rv[j][1]);
                }
                if (j != i) {
                    close(rv[j][0]);
                }
            }

            if (i == 0) {
                close(sv[i][0]);
                close(rv[i][1]);
                run_source(&ctx, sv[i], rv[i]);
                _exit(EXIT_SUCCESS);
            }

            if (i == child_count - 1) {
                close(sv[i - 1][1]);
                close(rv[i - 1][0]);
                run_sink(&ctx, i, sv[i - 1], rv[i - 1]);
                _exit(EXIT_SUCCESS);
            }

            close(sv[i - 1][1]);
            close(sv[i][0]);
            close(rv[i - 1][0]);
            close(rv[i][1]);
            run_middle(&ctx, i, sv[i - 1], sv[i], rv[i], rv[i - 1]);
            _exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < child_count - 1; ++i) {
        close_pipe_pair(sv[i]);
        close_pipe_pair(rv[i]);
    }

    free(sv);
    free(rv);

    while (wait(NULL) > 0) {
    }

    pipeline_unmap_shared_furniture(shared_furniture, furniture, furniture_count);
    return EXIT_SUCCESS;
}
