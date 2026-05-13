#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "pipeline_io.h"

void write_int(int fd, int value) {
    ssize_t written = write(fd, &value, sizeof(value));
    if (written != (ssize_t)sizeof(value)) {
        perror("write");
        exit(EXIT_FAILURE);
    }
}

int read_int(int fd, int *value) {
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

int read_int_nonblock(int fd, int *value) {
    ssize_t r = read(fd, value, sizeof(*value));
    if (r == (ssize_t)sizeof(*value)) {
        return 1;
    }
    if (r == 0) {
        return 0;
    }
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        return -1;
    }
    return -1;
}

void close_pipe_pair(int pipefd[2]) {
    close(pipefd[0]);
    close(pipefd[1]);
}
