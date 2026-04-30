/* ipc.c — message I/O, signal handling (self-pipe), serial generator. */
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

volatile sig_atomic_t g_flag_term     = 0;
volatile sig_atomic_t g_flag_reset    = 0;
volatile sig_atomic_t g_flag_newround = 0;
volatile sig_atomic_t g_flag_chld     = 0;

static int self_pipe_fds[2] = { -1, -1 };

int ipc_self_pipe_read_fd(void) { return self_pipe_fds[0]; }

/* Generic full-buffer read/write that retry on EINTR. */
static int full_write(int fd, const void *buf, size_t n) {
    const char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += w; left -= (size_t)w;
    }
    return 0;
}

static int full_read(int fd, void *buf, size_t n) {
    char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1; /* EOF before complete message */
        p += r; left -= (size_t)r;
    }
    return 0;
}

int msg_write(int fd, const Message *m) { return full_write(fd, m, sizeof(*m)); }
int msg_read (int fd, Message *m)       { return full_read (fd, m, sizeof(*m)); }
int evt_write(int fd, const Event *e)   { return full_write(fd, e, sizeof(*e)); }
int evt_read (int fd, Event *e)         { return full_read (fd, e, sizeof(*e)); }

/* Async-signal-safe handler: just set the flag and write 1 byte. */
static void on_signal(int sig) {
    int saved = errno;
    unsigned char b = (unsigned char)sig;
    if (self_pipe_fds[1] >= 0) {
        ssize_t r = write(self_pipe_fds[1], &b, 1);
        (void)r;
    }
    switch (sig) {
        case SIGINT:
        case SIGTERM:  g_flag_term     = 1; break;
        case SIGUSR1:  g_flag_reset    = 1; break;
        case SIGUSR2:  g_flag_newround = 1; break;
        case SIGCHLD:  g_flag_chld     = 1; break;
        default: break;
    }
    errno = saved;
}

static int set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

int ipc_install_handlers(void) {
    if (pipe(self_pipe_fds) < 0) return -1;
    set_nonblock(self_pipe_fds[0]);
    set_nonblock(self_pipe_fds[1]);
    /* Make sure the read fd is closed on exec — not strictly needed here. */
    fcntl(self_pipe_fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(self_pipe_fds[1], F_SETFD, FD_CLOEXEC);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    /* Important: SIGUSR1 must NOT have SA_RESTART so it interrupts read(). */
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    /* Ignore SIGPIPE: write() will return -1/EPIPE which we handle. */
    struct sigaction ign;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    sigaction(SIGPIPE, &ign, NULL);
    return 0;
}

int rand_range(unsigned *state, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rand_r(state) % (unsigned)(hi - lo + 1));
}

/* Fisher–Yates shuffle of [1..M] -> out[0..M-1]. The fill loop is OpenMP-able;
 * the shuffle itself stays serial because each step depends on a running RNG. */
void generate_unique_serials(int *out, int M, unsigned *rng_state, int openmp_enabled) {
    if (M <= 0) return;

#ifdef _OPENMP
    if (openmp_enabled) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < M; i++) out[i] = i + 1;
    } else
#endif
    {
        (void)openmp_enabled;
        for (int i = 0; i < M; i++) out[i] = i + 1;
    }

    /* Serial shuffle. */
    for (int i = M - 1; i > 0; i--) {
        int j = (int)(rand_r(rng_state) % (unsigned)(i + 1));
        int t = out[i]; out[i] = out[j]; out[j] = t;
    }
}
