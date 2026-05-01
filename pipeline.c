#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "constants.h"
#include "pipeline.h"
#include "furniture.h"

static volatile sig_atomic_t ack_flag = 0;
static pid_t g_source_pid = 0;
static furniture_piece *g_furniture = NULL;
static int g_furniture_count = 0;
static int g_expected_order = 0;
static int g_blocked_piece_index = -1;

static void sigusr1_handler(int sig) {
    (void)sig;
    ack_flag = 1;
}

static void write_int(int fd, int value) {
    ssize_t written = write(fd, &value, sizeof(value));
    if (written != (ssize_t)sizeof(value)) {
        perror("write");
        exit(EXIT_FAILURE);
    }
}

static int read_int(int fd, int *value) {
    char *buf = (char *)value;
    size_t received_total = 0;

    while (received_total < sizeof(*value)) {
        ssize_t r = read(fd, buf + received_total, sizeof(*value) - received_total);
        if (r == 0) {
            return 0;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            exit(EXIT_FAILURE);
        }
        received_total += (size_t)r;
    }

    return 1;
}

static void close_pipe_pair(int pipefd[2]) {
    close(pipefd[0]);
    close(pipefd[1]);
}

static void run_source(int forward_fd[2], int backward_fd[2]) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    printf("source (0) pile:");
    for (int i = 0; i < g_furniture_count; ++i) {
        printf(" %d", g_furniture[i].serial_no);
    }
    printf("\n");
    fflush(stdout);

    int *available_indices = malloc((size_t)g_furniture_count * sizeof(int));
    if (available_indices == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int placed_count = 0;
    while (placed_count < g_furniture_count) {
        int available_count = 0;
        for (int i = 0; i < g_furniture_count; ++i) {
            if (g_furniture[i].status == AVAILABLE) {
                available_indices[available_count++] = i;
            }
        }

        if (available_count == 0) {
            break;
        }

        int idx = available_indices[rand() % available_count];
        printf("source selected serial %d with status AVAILABLE\n", g_furniture[idx].serial_no);
        fflush(stdout);

        sigprocmask(SIG_BLOCK, &mask, &prev);
        ack_flag = 0;

        int serial = g_furniture[idx].serial_no;
        g_furniture[idx].status = MOVING_FORWARD;
        write_int(forward_fd[1], serial);
        printf("0 -> 1: serial=%d (order=%d, status=MOVING_FORWARD)\n", serial, g_furniture[idx].order);
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
            g_furniture[idx].status = PLACED;
            ++placed_count;
            printf("source (0) received ACK for serial: %d (status=PLACED)\n", serial);
            fflush(stdout);

            if (g_blocked_piece_index >= 0 && g_furniture[g_blocked_piece_index].status == BLOCKED) {
                g_furniture[g_blocked_piece_index].status = AVAILABLE;
                printf("source (0) released blocked serial: %d after successful placement\n",
                       g_furniture[g_blocked_piece_index].serial_no);
                fflush(stdout);
                furniture_display_table(g_furniture, g_furniture_count);
                g_blocked_piece_index = -1;
            }
        } else if (returned_serial == serial) {
            if (g_blocked_piece_index >= 0 && g_blocked_piece_index != idx && g_furniture[g_blocked_piece_index].status == BLOCKED) {
                g_furniture[g_blocked_piece_index].status = AVAILABLE;
                printf("source (0) replaced blocked serial: %d with serial: %d\n",
                       g_furniture[g_blocked_piece_index].serial_no, serial);
                fflush(stdout);
            }

            g_furniture[idx].status = BLOCKED;
            g_blocked_piece_index = idx;
            printf("source (0) received RETURNED serial: %d (status=BLOCKED)\n", serial);
            fflush(stdout);
            furniture_display_table(g_furniture, g_furniture_count);
        }

        sigprocmask(SIG_SETMASK, &prev, NULL);
    }

    free(available_indices);
    close(forward_fd[1]);
    close(backward_fd[0]);
}

static void run_sink(int index, int forward_fd[2], int backward_fd[2]) {
    int previous = index - 1;
    int serial = 0;
    int placed_count = 0;
    int returned_count = 0;

    while (read_int(forward_fd[0], &serial) == 1) {
        int furniture_idx = -1;
        int piece_order = -1;

        for (int i = 0; i < g_furniture_count; ++i) {
            if (g_furniture[i].serial_no == serial) {
                furniture_idx = i;
                piece_order = g_furniture[i].order;
                break;
            }
        }

        if (furniture_idx == -1) {
            printf("sink (%d): ERROR - piece not found\n", index);
            fflush(stdout);
            continue;
        }

        if (piece_order == g_expected_order) {
            ++placed_count;
            g_furniture[furniture_idx].status = PLACED;
            printf("sink (%d) <- %d: serial=%d (order=%d, CORRECT ORDER, status=PLACED)\n",
                   index, previous, serial, piece_order);
            printf("piece reached house\n");
            fflush(stdout);
            furniture_display_table(g_furniture, g_furniture_count);

            ++g_expected_order;

            if (g_source_pid > 0) {
                kill(g_source_pid, SIGUSR1);
                printf("%d -> source: SIGUSR1 (SUCCESS)\n", index);
                fflush(stdout);
            }
        } else {
            ++returned_count;
            g_furniture[furniture_idx].status = MOVING_BACKWARD;
            printf("sink (%d) <- %d: serial=%d (order=%d, WRONG ORDER expected %d, status=MOVING_BACKWARD)\n",
                   index, previous, serial, piece_order, g_expected_order);
            printf("sending piece back via backward socket\n");
            fflush(stdout);
            furniture_display_table(g_furniture, g_furniture_count);

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

static void run_middle(int index, int forward_in[2], int forward_out[2], int backward_in[2], int backward_out[2]) {
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

            for (int i = 0; i < g_furniture_count; ++i) {
                if (g_furniture[i].serial_no == serial) {
                    g_furniture[i].status = MOVING_FORWARD;
                    printf("%d <- %d: serial=%d (order=%d, status=MOVING_FORWARD)\n",
                           index, index - 1, serial, g_furniture[i].order);
                    fflush(stdout);
                    break;
                }
            }

            write_int(forward_out[1], serial);
            printf("%d -> %d: serial=%d (status=MOVING_FORWARD)\n", index, index + 1, serial);
            fflush(stdout);
        }

        if (FD_ISSET(backward_in[0], &readfds)) {
            if (read_int(backward_in[0], &serial) != 1) {
                continue;
            }

            for (int i = 0; i < g_furniture_count; ++i) {
                if (g_furniture[i].serial_no == serial) {
                    g_furniture[i].status = MOVING_BACKWARD;
                    printf("%d <- %d: serial=%d (order=%d, status=MOVING_BACKWARD)\n",
                           index, index + 1, serial, g_furniture[i].order);
                    fflush(stdout);
                    break;
                }
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

int run_pipeline(int child_count, furniture_piece *furniture, int furniture_count) {
    if (child_count < MIN_CHILDREN) {
        child_count = MIN_CHILDREN;
    }

    g_furniture = furniture;
    g_furniture_count = furniture_count;
    g_expected_order = 0;
    g_blocked_piece_index = -1;

    furniture_piece *shared_furniture = mmap(NULL,
                                             (size_t)furniture_count * sizeof(*shared_furniture),
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED | MAP_ANONYMOUS,
                                             -1,
                                             0);
    if (shared_furniture == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    memcpy(shared_furniture, furniture, (size_t)furniture_count * sizeof(*shared_furniture));
    g_furniture = shared_furniture;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    int (*sv)[2] = malloc((size_t)(child_count - 1) * sizeof(*sv));
    if (sv == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    int (*rv)[2] = malloc((size_t)(child_count - 1) * sizeof(*rv));
    if (rv == NULL) {
        perror("malloc");
        free(sv);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < child_count - 1; ++i) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv[i]) == -1) {
            perror("socketpair (forward)");
            for (int j = 0; j < i; ++j) {
                close_pipe_pair(sv[j]);
                close_pipe_pair(rv[j]);
            }
            free(sv);
            free(rv);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < child_count - 1; ++i) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, rv[i]) == -1) {
            perror("socketpair (backward)");
            for (int j = 0; j < child_count - 1; ++j) {
                close_pipe_pair(sv[j]);
                close_pipe_pair(rv[j]);
            }
            free(sv);
            free(rv);
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
            return EXIT_FAILURE;
        }

        if (i == 0 && pid > 0) {
            g_source_pid = pid;
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
                run_source(sv[i], rv[i]);
                _exit(EXIT_SUCCESS);
            }

            if (i == child_count - 1) {
                close(sv[i - 1][1]);
                close(rv[i - 1][0]);
                run_sink(i, sv[i - 1], rv[i - 1]);
                _exit(EXIT_SUCCESS);
            }

            close(sv[i - 1][1]);
            close(sv[i][0]);
            close(rv[i - 1][0]);
            close(rv[i][1]);
            run_middle(i, sv[i - 1], sv[i], rv[i], rv[i - 1]);
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

    munmap(shared_furniture, (size_t)furniture_count * sizeof(*shared_furniture));

    return EXIT_SUCCESS;
}