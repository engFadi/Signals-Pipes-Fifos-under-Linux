#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include "furniture.h"
#include "pipeline_io.h"
#include "pipeline_roles.h"
#include "pipeline_shared.h"

static volatile sig_atomic_t ack_flag = 0;

static void sigusr1_handler(int sig) {
    (void)sig;
    ack_flag = 1;
}

static void print_source_pile(PipelineContext *ctx) {
    printf("source (0) pile:");
    for (int i = 0; i < ctx->furniture_count; ++i) {
        printf(" %d", ctx->furniture[i].serial_no);
    }
    printf("\n");
    fflush(stdout);
}

void run_source(PipelineContext *ctx, int forward_fd[2], int backward_fd[2]) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    print_source_pile(ctx);

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
        printf("source selected serial %d with status AVAILABLE\n", ctx->furniture[idx].serial_no);
        fflush(stdout);

        sigprocmask(SIG_BLOCK, &mask, &prev);
        ack_flag = 0;

        int serial = ctx->furniture[idx].serial_no;
        ctx->furniture[idx].status = MOVING_FORWARD;
        write_int(forward_fd[1], serial);
        printf("0 -> 1: serial=%d (order=%d, status=MOVING_FORWARD)\n", serial, ctx->furniture[idx].order);
        fflush(stdout);

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
            printf("SUCCESS: serial %d placed. Releasing blocked pieces.\n", serial);
            fflush(stdout);

            int released = 0;
            for (int j = 0; j < ctx->furniture_count; ++j) {
                if (ctx->furniture[j].status == BLOCKED) {
                    ctx->furniture[j].status = AVAILABLE;
                    printf("Releasing blocked serial: %d\n", ctx->furniture[j].serial_no);
                    fflush(stdout);
                    ++released;
                }
            }

            if (released > 0) {
                furniture_display_table(ctx->furniture, ctx->furniture_count);
            }
        } else if (returned_serial == serial) {
            ctx->furniture[idx].status = BLOCKED;
            printf("RETURN: serial %d marked BLOCKED. No blocked pieces released.\n", serial);
            fflush(stdout);
            furniture_display_table(ctx->furniture, ctx->furniture_count);
        }

        sigprocmask(SIG_SETMASK, &prev, NULL);
    }

    free(available_indices);
    close(forward_fd[1]);
    close(backward_fd[0]);
}

void run_sink(PipelineContext *ctx, int index, int forward_fd[2], int backward_fd[2]) {
    int previous = index - 1;
    int serial = 0;
    int placed_count = 0;
    int returned_count = 0;

    while (read_int(forward_fd[0], &serial) == 1) {
        int furniture_idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
        if (furniture_idx == -1) {
            printf("sink (%d): ERROR - piece not found\n", index);
            fflush(stdout);
            continue;
        }

        int piece_order = ctx->furniture[furniture_idx].order;

        if (piece_order == ctx->expected_order) {
            ++placed_count;
            ctx->furniture[furniture_idx].status = PLACED;
            printf("sink (%d) <- %d: serial=%d (order=%d, CORRECT ORDER, status=PLACED)\n",
                   index, previous, serial, piece_order);
            printf("piece reached house\n");
            fflush(stdout);
            furniture_display_table(ctx->furniture, ctx->furniture_count);

            ++ctx->expected_order;

            if (ctx->source_pid > 0) {
                kill(ctx->source_pid, SIGUSR1);
                printf("%d -> source: SIGUSR1 (SUCCESS)\n", index);
                fflush(stdout);
            }
        } else {
            ++returned_count;
            ctx->furniture[furniture_idx].status = MOVING_BACKWARD;
            printf("sink (%d) <- %d: serial=%d (order=%d, WRONG ORDER expected %d, status=MOVING_BACKWARD)\n",
                   index, previous, serial, piece_order, ctx->expected_order);
            printf("sending piece back via backward socket\n");
            fflush(stdout);
            furniture_display_table(ctx->furniture, ctx->furniture_count);

            write_int(backward_fd[1], serial);
            printf("%d -> %d: serial=%d (status=MOVING_BACKWARD)\n", index, previous, serial);
            fflush(stdout);
        }
    }

    printf("sink (%d) completed: %d correct pieces, %d returned\n", index, placed_count, returned_count);
    fflush(stdout);
    close(forward_fd[0]);
    close(backward_fd[1]);
}

void run_middle(PipelineContext *ctx,
                int index,
                int forward_in[2],
                int forward_out[2],
                int backward_in[2],
                int backward_out[2]) {
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

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_FORWARD;
                printf("%d <- %d: serial=%d (order=%d, status=MOVING_FORWARD)\n",
                       index, index - 1, serial, ctx->furniture[idx].order);
                fflush(stdout);
            }

            write_int(forward_out[1], serial);
            printf("%d -> %d: serial=%d (status=MOVING_FORWARD)\n", index, index + 1, serial);
            fflush(stdout);
        }

        if (FD_ISSET(backward_in[0], &readfds)) {
            if (read_int(backward_in[0], &serial) != 1) {
                continue;
            }

            int idx = pipeline_find_piece_index(ctx->furniture, ctx->furniture_count, serial);
            if (idx >= 0) {
                ctx->furniture[idx].status = MOVING_BACKWARD;
                printf("%d <- %d: serial=%d (order=%d, status=MOVING_BACKWARD)\n",
                       index, index + 1, serial, ctx->furniture[idx].order);
                fflush(stdout);
            }

            write_int(backward_out[1], serial);
            printf("%d -> %d: serial=%d (status=MOVING_BACKWARD)\n", index, index - 1, serial);
            fflush(stdout);
        }
    }

    close(forward_in[0]);
    close(forward_out[1]);
    close(backward_in[0]);
    close(backward_out[1]);
}
