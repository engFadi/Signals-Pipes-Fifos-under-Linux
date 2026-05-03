#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
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

static void sleep_for_pause(double pause_seconds) {
    if (pause_seconds < 0.0) {
        return;
    }

    struct timespec request;
    request.tv_sec = (time_t)pause_seconds;
    request.tv_nsec = (long)((pause_seconds - (double)request.tv_sec) * 1000000000.0);
    if (request.tv_nsec < 0) {
        request.tv_nsec = 0;
    }

    while (nanosleep(&request, &request) == -1 && errno == EINTR) {
    }
}

static void apply_member_pause(PipelineContext *ctx) {
    double chosen_pause = random_pause_between(ctx->current_min_pause, ctx->current_max_pause);
    sleep_for_pause(chosen_pause);
    ctx->current_min_pause += FATIGUE_STEP;
    ctx->current_max_pause += FATIGUE_STEP;
}

static void sigusr1_handler(int sig) {
    (void)sig;
    ack_flag = 1;
}

void run_source(PipelineContext *ctx, int forward_fd[2], int backward_fd[2]) {
    seed_process_random(0);

    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

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

        printf("[T%d] Source picked serial %d (order %d)\n",
               ctx->team_id, ctx->furniture[idx].serial_no, ctx->furniture[idx].order);
        fflush(stdout);

        apply_member_pause(ctx);

        sigprocmask(SIG_BLOCK, &mask, &prev);
        ack_flag = 0;

        int serial = ctx->furniture[idx].serial_no;
        ctx->furniture[idx].status = MOVING_FORWARD;
        write_int(forward_fd[1], serial);

        int returned_serial = -1;
        while (!ack_flag && returned_serial == -1) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(backward_fd[0], &readfds);

            sigprocmask(SIG_SETMASK, &prev, NULL);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;

            int select_ready = select(backward_fd[0] + 1, &readfds, NULL, NULL, &tv);
            if (select_ready > 0 && FD_ISSET(backward_fd[0], &readfds)) {
                if (read_int(backward_fd[0], &returned_serial) == 1) {
                    break;
                }
            }

            sigprocmask(SIG_BLOCK, &mask, NULL);
        }

        if (ack_flag) {
            ctx->furniture[idx].status = PLACED;
            ++placed_count;
            printf("[T%d] >> Serial %d placed (order %d) -- %d/%d done\n",
                   ctx->team_id, serial, ctx->furniture[idx].order,
                   placed_count, ctx->furniture_count);
            fflush(stdout);

            int released = 0;
            for (int j = 0; j < ctx->furniture_count; ++j) {
                if (ctx->furniture[j].status == BLOCKED) {
                    ctx->furniture[j].status = AVAILABLE;
                    ++released;
                }
            }

            if (released > 0) {
                printf("[T%d] Released %d blocked pieces\n", ctx->team_id, released);
                fflush(stdout);
            }
        } else if (returned_serial == serial) {
            apply_member_pause(ctx);
            ctx->furniture[idx].status = BLOCKED;
            printf("[T%d] << Serial %d returned -> BLOCKED\n",
                   ctx->team_id, serial);
            fflush(stdout);
        }

        sigprocmask(SIG_SETMASK, &prev, NULL);
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

        apply_member_pause(ctx);

        if (piece_order == ctx->expected_order) {
            ctx->furniture[furniture_idx].status = PLACED;

            ++ctx->expected_order;

            if (ctx->source_pid > 0) {
                kill(ctx->source_pid, SIGUSR1);
            }
        } else {
            ctx->furniture[furniture_idx].status = MOVING_BACKWARD;
            printf("[T%d] x Serial %d rejected (order %d, expected %d) -- sending back\n",
                   ctx->team_id, serial, piece_order, ctx->expected_order);
            fflush(stdout);

            write_int(backward_fd[1], serial);
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

    fd_set readfds;
    int max_fd = (forward_in[0] > backward_in[0]) ? forward_in[0] : backward_in[0];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(forward_in[0], &readfds);
        FD_SET(backward_in[0], &readfds);

        int select_result = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (select_result <= 0) {
            continue;
        }

        int serial = 0;

        if (FD_ISSET(forward_in[0], &readfds)) {
            if (read_int(forward_in[0], &serial) != 1) {
                break;
            }

            apply_member_pause(ctx);

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_FORWARD;
            }

            write_int(forward_out[1], serial);
        }

        if (FD_ISSET(backward_in[0], &readfds)) {
            if (read_int(backward_in[0], &serial) != 1) {
                continue;
            }

            apply_member_pause(ctx);

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_BACKWARD;
            }

            write_int(backward_out[1], serial);
        }
    }

    close(forward_in[0]);
    close(forward_out[1]);
    close(backward_in[0]);
    close(backward_out[1]);
}
