#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "furniture.h"
#include "pipeline_io.h"
#include "pipeline_roles.h"
#include "pipeline_shared.h"

static volatile sig_atomic_t ack_flag = 0;

static void seed_process_random(unsigned int member_id) {
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid() ^ (member_id * 2654435761u);
    srand(seed);
}

static double random_pause_between(double min_pause, double max_pause) {
    if (max_pause < min_pause) {
        double tmp = min_pause;
        min_pause = max_pause;
        max_pause = tmp;
    }

    double fraction = (double)rand() / ((double)RAND_MAX + 1.0);
    return min_pause + fraction * (max_pause - min_pause);
}

static void apply_member_pause(PipelineContext *ctx) {
    double chosen_pause = random_pause_between(ctx->current_min_pause, ctx->current_max_pause);
    if (chosen_pause > 0.0) {
        usleep((useconds_t)(chosen_pause * 1000000.0));
    }
    ctx->current_min_pause += FATIGUE_STEP;
    ctx->current_max_pause += FATIGUE_STEP;
}

static double take_member_pause(PipelineContext *ctx) {
    double chosen_pause = random_pause_between(ctx->current_min_pause, ctx->current_max_pause);
    ctx->current_min_pause += FATIGUE_STEP;
    ctx->current_max_pause += FATIGUE_STEP;
    return chosen_pause;
}

static void sleep_member_pause(double pause_seconds) {
    if (pause_seconds > 0.0) {
        usleep((useconds_t)(pause_seconds * 1000000.0));
    }
}

static void sigusr1_handler(int sig) {
    (void)sig;
    ack_flag = 1;
}

static void set_sigusr1_blocked(int blocked) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(blocked ? SIG_BLOCK : SIG_UNBLOCK, &set, NULL);
}

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void log_and_visual_event(PipelineContext *ctx,
                                 int serial,
                                 int from_node,
                                 int to_node,
                                 PipelineEventType event_type) {
    int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
    int order = (idx >= 0) ? ctx->furniture[idx].order : -1;

    switch (event_type) {
        case PIPE_EVENT_PICKED:
            printf("[T%d] Source picked serial %d (order %d)\n",
                   ctx->team_id, serial, order);
            break;
        case PIPE_EVENT_FORWARD:
            printf("[T%d] %d -> %d: serial %d moving forward\n",
                   ctx->team_id, from_node, to_node, serial);
            break;
        case PIPE_EVENT_BACKWARD:
            printf("[T%d] %d -> %d: serial %d moving backward\n",
                   ctx->team_id, from_node, to_node, serial);
            break;
        case PIPE_EVENT_PLACED:
            printf("[T%d] >> Serial %d placed (order %d)\n",
                   ctx->team_id, serial, order);
            break;
        case PIPE_EVENT_REJECTED:
            printf("[T%d] x Serial %d rejected (order %d, expected %d) -- sending back\n",
                   ctx->team_id, serial, order, ctx->expected_order);
            break;
        case PIPE_EVENT_BLOCKED:
            printf("[T%d] << Serial %d returned -> BLOCKED\n",
                   ctx->team_id, serial);
            break;
        case PIPE_EVENT_RELEASED:
            printf("[T%d] Released blocked serial %d\n",
                   ctx->team_id, serial);
            break;
    }
    fflush(stdout);

}

static void queue_visual_hop(PipelineContext *ctx,
                             int serial,
                             int from_node,
                             int to_node,
                             PipelineEventType event_type,
                             double duration_seconds) {
    int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);

    if (idx >= 0 && from_node >= 0 && to_node >= 0 && from_node != to_node) {
        piece_status visual_status =
            (event_type == PIPE_EVENT_BACKWARD || event_type == PIPE_EVENT_REJECTED)
                ? MOVING_BACKWARD
                : MOVING_FORWARD;
        visual_queue_hop(ctx->visual, ctx->team_id, idx,
                         from_node, to_node, visual_status, duration_seconds);
    }
}

void run_source(PipelineContext *ctx, int forward_fd[2], int backward_fd[2]) {
    seed_process_random(0);

    signal(SIGUSR1, sigusr1_handler);

    set_nonblocking(backward_fd[0]);

    int *available_indices = malloc((size_t)ctx->furniture_count * sizeof(int));
    if (available_indices == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int placed_count = 0;
    while (placed_count < ctx->furniture_count) {
        int available_count = 0;
        for (int i = 0; i < ctx->furniture_count; ++i) {
            if (ctx->furniture[i].status == AVAILABLE) {
                available_indices[available_count++] = i;
            }
        }

        if (available_count == 0) {
            break;
        }

        int idx = available_indices[rand() % available_count];

        log_and_visual_event(ctx, ctx->furniture[idx].serial_no,
                             -1, -1, PIPE_EVENT_PICKED);

        set_sigusr1_blocked(1);
        ack_flag = 0;

        int serial = ctx->furniture[idx].serial_no;
        ctx->furniture[idx].status = MOVING_FORWARD;
        double pause = take_member_pause(ctx);
        queue_visual_hop(ctx, serial, 0, 1, PIPE_EVENT_FORWARD, pause);
        sleep_member_pause(pause);
        write_int(forward_fd[1], serial);
        log_and_visual_event(ctx, serial, 0, 1, PIPE_EVENT_FORWARD);

        set_sigusr1_blocked(0);

        int returned_serial = -1;
        while (!ack_flag && returned_serial == -1) {
            if (read_int_nonblock(backward_fd[0], &returned_serial) == 1) {
                break;
            }
            usleep(1000);
        }

        set_sigusr1_blocked(1);

        if (ack_flag) {
            ctx->furniture[idx].status = PLACED;
            ++placed_count;
            log_and_visual_event(ctx, serial, -1, -1, PIPE_EVENT_PLACED);

            for (int j = 0; j < ctx->furniture_count; ++j) {
                if (ctx->furniture[j].status == BLOCKED) {
                    ctx->furniture[j].status = AVAILABLE;
                    log_and_visual_event(ctx, ctx->furniture[j].serial_no,
                                         -1, -1, PIPE_EVENT_RELEASED);
                }
            }

        } else if (returned_serial == serial) {
            apply_member_pause(ctx);
            ctx->furniture[idx].status = BLOCKED;
            log_and_visual_event(ctx, serial, -1, -1, PIPE_EVENT_BLOCKED);
        }

        set_sigusr1_blocked(0);
    }

    free(available_indices);
    close(forward_fd[1]);
    close(backward_fd[0]);
}

void run_sink(PipelineContext *ctx, int index, int forward_fd[2], int backward_fd[2]) {
    seed_process_random((unsigned int)index);

    int serial = 0;

    while (read_int(forward_fd[0], &serial) == 1) {
        int furniture_idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
        if (furniture_idx == -1) {
            continue;
        }

        int piece_order = ctx->furniture[furniture_idx].order;

        double pause = take_member_pause(ctx);

        if (piece_order == ctx->expected_order) {
            sleep_member_pause(pause);
            ctx->furniture[furniture_idx].status = PLACED;

            ++ctx->expected_order;

            if (ctx->source_pid > 0) {
                kill(ctx->source_pid, SIGUSR1);
            }
        } else {
            ctx->furniture[furniture_idx].status = MOVING_BACKWARD;
            queue_visual_hop(ctx, serial, index, index - 1,
                             PIPE_EVENT_REJECTED, pause);
            sleep_member_pause(pause);
            write_int(backward_fd[1], serial);
            log_and_visual_event(ctx, serial, index, index - 1,
                                 PIPE_EVENT_REJECTED);
        }
    }

    close(forward_fd[0]);
    close(backward_fd[1]);
}

void run_middle(PipelineContext *ctx,
                int index,
                int forward_in[2],
                int forward_out[2],
                int backward_in[2],
                int backward_out[2]) {
    seed_process_random((unsigned int)index);

    set_nonblocking(forward_in[0]);
    set_nonblocking(backward_in[0]);

    while (1) {
        int serial = 0;
        int got_forward = 0;
        int got_backward = 0;

        int rf = read_int_nonblock(forward_in[0], &serial);
        if (rf == 1) {
            got_forward = 1;
        } else if (rf == 0) {
            break;
        }

        if (got_forward) {
            double pause = take_member_pause(ctx);

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_FORWARD;
                queue_visual_hop(ctx, serial, index, index + 1,
                                 PIPE_EVENT_FORWARD, pause);
            }

            sleep_member_pause(pause);
            write_int(forward_out[1], serial);
            if (idx >= 0) {
                log_and_visual_event(ctx, serial, index, index + 1,
                                     PIPE_EVENT_FORWARD);
            }
        }

        int serial_back = 0;
        int rb = read_int_nonblock(backward_in[0], &serial_back);
        if (rb == 1) {
            got_backward = 1;
        }

        if (got_backward) {
            double pause = take_member_pause(ctx);

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial_back);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_BACKWARD;
                queue_visual_hop(ctx, serial_back, index, index - 1,
                                 PIPE_EVENT_BACKWARD, pause);
            }

            sleep_member_pause(pause);
            write_int(backward_out[1], serial_back);
            if (idx >= 0) {
                log_and_visual_event(ctx, serial_back, index, index - 1,
                                     PIPE_EVENT_BACKWARD);
            }
        }

        if (!got_forward && !got_backward) {
            usleep(1000);
        }
    }

    close(forward_in[0]);
    close(forward_out[1]);
    close(backward_in[0]);
    close(backward_out[1]);
}
