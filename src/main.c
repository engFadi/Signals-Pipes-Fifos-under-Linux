/* main.c — referee process: build IPC topology, fork members, run rounds. */
#include "config.h"
#include "ipc.h"
#include "team.h"
#include "graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#define NUM_TEAMS 2

extern volatile sig_atomic_t g_flag_term;
extern volatile sig_atomic_t g_flag_chld;

typedef struct {
    int  team_id;
    int  N;
    pid_t *pids;        /* length N */
    int (*fwd)[2];      /* N-1 pipes: fwd[k][0]=read, fwd[k][1]=write */
    int (*bwd)[2];      /* N-1 pipes */
    int  wins;
} Team;

static Team teams[NUM_TEAMS];
static int  fifo_fd_r = -1;     /* referee read end of FIFO */
static int  fifo_fd_w = -1;     /* opened so the FIFO has a writer (avoids EOF storms) */
static char fifo_path_buf[256];

static void teardown(void);

/* --------------------------- pipe topology ------------------------------- */

static int build_team_pipes(Team *t, int N) {
    t->N = N;
    t->wins = 0;
    t->pids = calloc((size_t)N, sizeof(pid_t));
    t->fwd  = calloc((size_t)(N - 1), sizeof(int[2]));
    t->bwd  = calloc((size_t)(N - 1), sizeof(int[2]));
    if (!t->pids || !t->fwd || !t->bwd) return -1;
    for (int k = 0; k < N - 1; k++) {
        if (pipe(t->fwd[k]) < 0) return -1;
        if (pipe(t->bwd[k]) < 0) return -1;
    }
    return 0;
}

static void close_all_team_pipes(Team *t) {
    for (int k = 0; k < t->N - 1; k++) {
        if (t->fwd[k][0] >= 0) close(t->fwd[k][0]);
        if (t->fwd[k][1] >= 0) close(t->fwd[k][1]);
        if (t->bwd[k][0] >= 0) close(t->bwd[k][0]);
        if (t->bwd[k][1] >= 0) close(t->bwd[k][1]);
        t->fwd[k][0] = t->fwd[k][1] = -1;
        t->bwd[k][0] = t->bwd[k][1] = -1;
    }
}

/* --------------------------- referee FIFO -------------------------------- */

static int open_referee_fifo(const char *path) {
    /* Best-effort cleanup. */
    unlink(path);
    if (mkfifo(path, 0600) < 0 && errno != EEXIST) {
        fprintf(stderr, "mkfifo %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    int rfd = open(path, O_RDONLY | O_NONBLOCK);
    if (rfd < 0) {
        fprintf(stderr, "open(FIFO,RDONLY) failed: %s\n", strerror(errno));
        return -1;
    }
    /* Open a permanent writer so reads never see EOF when no sink has opened yet. */
    int wfd = open(path, O_WRONLY);
    if (wfd < 0) {
        fprintf(stderr, "open(FIFO,WRONLY) failed: %s\n", strerror(errno));
        close(rfd);
        return -1;
    }
    /* Switch read end back to blocking — we'll use poll() anyway. */
    int fl = fcntl(rfd, F_GETFL, 0);
    fcntl(rfd, F_SETFL, fl & ~O_NONBLOCK);
    fifo_fd_r = rfd;
    fifo_fd_w = wfd;
    return 0;
}

/* --------------------------- send helpers -------------------------------- */

static void send_init_to_member(int fd, int round_id, const int *serials, int M) {
    Message init = { .type = MSG_INIT, .round_id = round_id, .payload = M };
    msg_write(fd, &init);
    for (int i = 0; i < M; i++) {
        Message ser = { .type = MSG_PIECE, .piece_id = serials[i],
                        .direction = DIR_FWD, .round_id = round_id, .status = STAT_OK };
        msg_write(fd, &ser);
    }
}

/* Wait for an ACK of given type on the source's bwd_in (referee owns the read end). */
static int wait_for_ack(int fd, int wanted_type, int timeout_ms) {
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int waited = 0;
    while (waited < timeout_ms || timeout_ms < 0) {
        int pr = poll(&pfd, 1, 200);
        if (g_flag_term) return -1;
        if (pr < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (pr == 0) { waited += 200; continue; }
        Message m;
        if (msg_read(fd, &m) < 0) return -1;
        if (m.type == wanted_type) return 0;
        /* Otherwise, keep looking. */
    }
    return -1;
}

/* --------------------------- forking ------------------------------------- */

#if 0  /* superseded by the inline forking in main(); kept for reference */
static pid_t fork_member(Team *t, int idx, const Config *cfg, unsigned base_seed) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) { t->pids[idx] = pid; return pid; }

    /* In child. */
    MemberCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.member_index = idx;
    ctx.team_id = t->team_id;
    ctx.N = t->N;
    ctx.cfg = cfg;
    ctx.rng_state = base_seed ^ ((unsigned)t->team_id * 2654435761u)
                              ^ ((unsigned)idx * 40503u)
                              ^ (unsigned)getpid();
    /* Resolve fds. */
    ctx.fwd_in  = (idx == 0)         ? -1 : t->fwd[idx - 1][0];
    ctx.fwd_out = (idx == t->N - 1)  ? -1 : t->fwd[idx][1];
    ctx.bwd_in  = (idx == t->N - 1)  ? -1 : t->bwd[idx][0];
    ctx.bwd_out = (idx == 0)         ? -1 : t->bwd[idx - 1][1];
    ctx.referee_fifo_w = -1;

    /* Source's fwd_in is fed by the referee directly via fwd[0][1] which the
     * referee keeps open as a writer (we'll open a referee→source channel
     * differently). To keep one topology only, we instead use a dedicated
     * referee→source pipe stored in fwd[-1]: see main(), where we add an
     * extra pipe by making fwd[0] connect referee→source rather than
     * source→member1. To avoid that special case, the referee keeps the
     * write end of fwd[0] open and writes INITs there before the source
     * attempts to read. The source is also the reader of fwd[0]. */
    /* Implementation: source's fwd_in is fwd[0][0] (read end of pipe to
     * member 1). But that pipe normally goes source→member1. We treat the
     * INIT phase by re-using a dedicated init pipe: see init_pipes below. */

    /* Close every pipe in every team except my four. */
    for (int tid = 0; tid < NUM_TEAMS; tid++) {
        Team *ot = &teams[tid];
        for (int k = 0; k < ot->N - 1; k++) {
            int *fwd = ot->fwd[k];
            int *bwd = ot->bwd[k];
            if (fwd[0] != ctx.fwd_in)  close(fwd[0]);
            if (fwd[1] != ctx.fwd_out) close(fwd[1]);
            if (bwd[0] != ctx.bwd_in)  close(bwd[0]);
            if (bwd[1] != ctx.bwd_out) close(bwd[1]);
        }
    }
    if (fifo_fd_r >= 0) close(fifo_fd_r);
    /* sink keeps the FIFO writer */
    if (idx == t->N - 1 && fifo_fd_w >= 0) {
        ctx.referee_fifo_w = fifo_fd_w;
    } else {
        if (fifo_fd_w >= 0) close(fifo_fd_w);
    }

    /* The "INIT" stream into the source needs to come from the referee.
     * We solve this by NOT giving the source a fwd_in pipe coming from
     * "previous member" (there is none). Instead, the referee owns the
     * write end of an extra pipe whose read end is the source's fwd_in.
     * That extra pipe is built in main() and stashed in t->bwd[N-1]?  No,
     * we pre-allocate it as a separate per-team init pipe. See main(). */
    int rc;
    if (idx == 0)              rc = team_run_source(&ctx);
    else if (idx == t->N - 1)  rc = team_run_sink(&ctx);
    else                       rc = team_run_middle(&ctx);
    _exit(rc);
}
#endif

/* Per-team init pipe: referee → source.
 *   init_pipe[t][0] = read end (handed to source as fwd_in)
 *   init_pipe[t][1] = write end (kept by referee) */
static int init_pipe[NUM_TEAMS][2];
/* Per-team source-ack pipe: source → referee.
 *   ack_pipe[t][0] = read end (referee), ack_pipe[t][1] = write end (source) */
static int ack_pipe[NUM_TEAMS][2];
/* Per-team sink-init pipe: referee → sink (so sink also gets the serial list). */
static int sink_init_pipe[NUM_TEAMS][2];

/* --------------------------- main ---------------------------------------- */

static void teardown(void) {
    /* Send TERMINATE to every source via init pipe and to every sink directly. */
    Message term = { .type = MSG_TERMINATE };
    for (int tid = 0; tid < NUM_TEAMS; tid++) {
        if (init_pipe[tid][1] >= 0) msg_write(init_pipe[tid][1], &term);
        if (sink_init_pipe[tid][1] >= 0) msg_write(sink_init_pipe[tid][1], &term);
    }
    /* Then SIGTERM. */
    for (int tid = 0; tid < NUM_TEAMS; tid++) {
        Team *t = &teams[tid];
        if (!t->pids) continue;
        for (int i = 0; i < t->N; i++) {
            if (t->pids[i] > 0) kill(t->pids[i], SIGTERM);
        }
    }
    /* Reap. */
    for (int tid = 0; tid < NUM_TEAMS; tid++) {
        Team *t = &teams[tid];
        if (!t->pids) continue;
        for (int i = 0; i < t->N; i++) {
            if (t->pids[i] > 0) {
                int st;
                waitpid(t->pids[i], &st, 0);
            }
        }
    }
    if (fifo_fd_r >= 0) close(fifo_fd_r);
    if (fifo_fd_w >= 0) close(fifo_fd_w);
    if (fifo_path_buf[0]) unlink(fifo_path_buf);

    for (int tid = 0; tid < NUM_TEAMS; tid++) {
        if (init_pipe[tid][0] >= 0) close(init_pipe[tid][0]);
        if (init_pipe[tid][1] >= 0) close(init_pipe[tid][1]);
        if (ack_pipe[tid][0] >= 0)  close(ack_pipe[tid][0]);
        if (ack_pipe[tid][1] >= 0)  close(ack_pipe[tid][1]);
        if (sink_init_pipe[tid][0] >= 0) close(sink_init_pipe[tid][0]);
        if (sink_init_pipe[tid][1] >= 0) close(sink_init_pipe[tid][1]);
        close_all_team_pipes(&teams[tid]);
        free(teams[tid].pids);
        free(teams[tid].fwd);
        free(teams[tid].bwd);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <config.txt>\n", argv[0]);
        return 2;
    }
    Config cfg;
    if (config_load(argv[1], &cfg) < 0) return 2;
    config_print(&cfg);

#ifdef _OPENMP
    if (cfg.openmp_enabled) {
        fprintf(stderr, "[omp] enabled, max_threads=%d\n", omp_get_max_threads());
    } else {
        omp_set_num_threads(1);
        fprintf(stderr, "[omp] disabled at runtime (single thread)\n");
    }
#endif

    if (ipc_install_handlers() < 0) {
        fprintf(stderr, "signal install failed\n");
        return 2;
    }

    unsigned seed = cfg.random_seed ? cfg.random_seed : (unsigned)time(NULL);
    srand(seed);

    /* Init pipe arrays to -1. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        init_pipe[t][0] = init_pipe[t][1] = -1;
        ack_pipe[t][0] = ack_pipe[t][1] = -1;
        sink_init_pipe[t][0] = sink_init_pipe[t][1] = -1;
        memset(&teams[t], 0, sizeof(teams[t]));
        teams[t].team_id = t;
    }

    strncpy(fifo_path_buf, cfg.referee_fifo_path, sizeof(fifo_path_buf) - 1);
    if (open_referee_fifo(cfg.referee_fifo_path) < 0) { teardown(); return 2; }

    /* Build the per-team forward/backward pipes and the special init/ack pipes. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        if (build_team_pipes(&teams[t], cfg.team_member_count) < 0) {
            fprintf(stderr, "pipe alloc failed\n"); teardown(); return 2;
        }
        if (pipe(init_pipe[t]) < 0 || pipe(ack_pipe[t]) < 0 ||
            pipe(sink_init_pipe[t]) < 0) {
            perror("pipe"); teardown(); return 2;
        }
    }

    (void)graphics_spawn_viewer(&cfg);   /* no-op stub */

    /* Fork all members. We need to wire init/ack pipes specially:
     *   source's fwd_in   ← init_pipe[t][0]    (referee writes INITs / TERM there)
     *   source's bwd_out  ← ack_pipe[t][1]     (overrides the natural bwd_out)
     *   sink's fwd_in     ← we keep both: regular fwd_in (from prev member) for
     *                       data, and an extra "sink_init" channel.
     * For simplicity we instead give sinks their order list also through the
     * normal forward path: each middle just propagates MSG_INIT. So we
     * actually only need init_pipe (referee→source) and ack_pipe (source→referee).
     * Drop sink_init_pipe. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        close(sink_init_pipe[t][0]); close(sink_init_pipe[t][1]);
        sink_init_pipe[t][0] = sink_init_pipe[t][1] = -1;
    }

    /* Now actually fork. We bypass the natural source.fwd_in/bwd_out by
     * temporarily overriding teams[t].fwd[0][0] read-end and bwd[0][1]
     * write-end with the init/ack pipes for the source's view. The cleanest
     * way is to fork sources separately with custom contexts. Let's just do
     * that here. */

    /* Fork middles + sinks normally first. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        for (int i = 1; i < cfg.team_member_count; i++) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); teardown(); return 2; }
            if (pid == 0) {
                /* Child: prepare ctx. */
                MemberCtx ctx;
                memset(&ctx, 0, sizeof(ctx));
                ctx.member_index = i;
                ctx.team_id = t;
                ctx.N = cfg.team_member_count;
                ctx.cfg = &cfg;
                ctx.rng_state = seed ^ ((unsigned)t * 2654435761u)
                                     ^ ((unsigned)i * 40503u)
                                     ^ (unsigned)getpid();
                ctx.fwd_in  = teams[t].fwd[i - 1][0];
                ctx.fwd_out = (i == cfg.team_member_count - 1) ? -1 : teams[t].fwd[i][1];
                ctx.bwd_in  = (i == cfg.team_member_count - 1) ? -1 : teams[t].bwd[i][0];
                ctx.bwd_out = teams[t].bwd[i - 1][1];
                ctx.referee_fifo_w = -1;

                /* Close everything else. */
                for (int tid = 0; tid < NUM_TEAMS; tid++) {
                    Team *ot = &teams[tid];
                    for (int k = 0; k < ot->N - 1; k++) {
                        if (ot->fwd[k][0] != ctx.fwd_in)  close(ot->fwd[k][0]);
                        if (ot->fwd[k][1] != ctx.fwd_out) close(ot->fwd[k][1]);
                        if (ot->bwd[k][0] != ctx.bwd_in)  close(ot->bwd[k][0]);
                        if (ot->bwd[k][1] != ctx.bwd_out) close(ot->bwd[k][1]);
                    }
                    /* close init/ack pipes */
                    if (init_pipe[tid][0] >= 0) close(init_pipe[tid][0]);
                    if (init_pipe[tid][1] >= 0) close(init_pipe[tid][1]);
                    if (ack_pipe[tid][0] >= 0)  close(ack_pipe[tid][0]);
                    if (ack_pipe[tid][1] >= 0)  close(ack_pipe[tid][1]);
                }
                if (i == cfg.team_member_count - 1) {
                    /* sink: needs FIFO writer, close the referee's read end. */
                    if (fifo_fd_r >= 0) close(fifo_fd_r);
                    ctx.referee_fifo_w = fifo_fd_w;
                } else {
                    if (fifo_fd_r >= 0) close(fifo_fd_r);
                    if (fifo_fd_w >= 0) close(fifo_fd_w);
                }

                int rc;
                if (i == cfg.team_member_count - 1) rc = team_run_sink(&ctx);
                else                                rc = team_run_middle(&ctx);
                _exit(rc);
            }
            teams[t].pids[i] = pid;
        }
    }

    /* Fork sources. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); teardown(); return 2; }
        if (pid == 0) {
            MemberCtx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.member_index = 0;
            ctx.team_id = t;
            ctx.N = cfg.team_member_count;
            ctx.cfg = &cfg;
            ctx.rng_state = seed ^ ((unsigned)t * 2654435761u)
                                 ^ (unsigned)getpid();

            /* Source's "fwd_in" is the init_pipe (referee→source). */
            ctx.fwd_in  = init_pipe[t][0];
            ctx.fwd_out = teams[t].fwd[0][1];
            ctx.bwd_in  = teams[t].bwd[0][0];
            /* Source's "bwd_out" goes to the referee via ack_pipe. */
            ctx.bwd_out = ack_pipe[t][1];
            ctx.referee_fifo_w = -1;

            /* Close all unrelated fds. */
            for (int tid = 0; tid < NUM_TEAMS; tid++) {
                Team *ot = &teams[tid];
                for (int k = 0; k < ot->N - 1; k++) {
                    if (ot->fwd[k][0] != ctx.fwd_in && ot->fwd[k][0] != ctx.fwd_out
                        && ot->fwd[k][0] != ctx.bwd_in && ot->fwd[k][0] != ctx.bwd_out)
                        close(ot->fwd[k][0]);
                    if (ot->fwd[k][1] != ctx.fwd_in && ot->fwd[k][1] != ctx.fwd_out
                        && ot->fwd[k][1] != ctx.bwd_in && ot->fwd[k][1] != ctx.bwd_out)
                        close(ot->fwd[k][1]);
                    if (ot->bwd[k][0] != ctx.fwd_in && ot->bwd[k][0] != ctx.fwd_out
                        && ot->bwd[k][0] != ctx.bwd_in && ot->bwd[k][0] != ctx.bwd_out)
                        close(ot->bwd[k][0]);
                    if (ot->bwd[k][1] != ctx.fwd_in && ot->bwd[k][1] != ctx.fwd_out
                        && ot->bwd[k][1] != ctx.bwd_in && ot->bwd[k][1] != ctx.bwd_out)
                        close(ot->bwd[k][1]);
                }
                /* close init/ack pipes that aren't ours, and unused ends of ours */
                for (int kt = 0; kt < NUM_TEAMS; kt++) {
                    if (kt == t) {
                        /* we own init_pipe[t][0] (read) and ack_pipe[t][1] (write) */
                        if (init_pipe[kt][1] >= 0) close(init_pipe[kt][1]);
                        if (ack_pipe[kt][0]  >= 0) close(ack_pipe[kt][0]);
                    } else {
                        if (init_pipe[kt][0] >= 0) close(init_pipe[kt][0]);
                        if (init_pipe[kt][1] >= 0) close(init_pipe[kt][1]);
                        if (ack_pipe[kt][0]  >= 0) close(ack_pipe[kt][0]);
                        if (ack_pipe[kt][1]  >= 0) close(ack_pipe[kt][1]);
                    }
                }
            }
            if (fifo_fd_r >= 0) close(fifo_fd_r);
            if (fifo_fd_w >= 0) close(fifo_fd_w);

            int rc = team_run_source(&ctx);
            _exit(rc);
        }
        teams[t].pids[0] = pid;
    }

    /* Referee closes ends it doesn't own. */
    for (int t = 0; t < NUM_TEAMS; t++) {
        /* referee owns init_pipe[t][1] (write) and ack_pipe[t][0] (read) */
        close(init_pipe[t][0]);    init_pipe[t][0] = -1;
        close(ack_pipe[t][1]);     ack_pipe[t][1]  = -1;
        /* referee doesn't need any fwd/bwd ends */
        for (int k = 0; k < teams[t].N - 1; k++) {
            close(teams[t].fwd[k][0]); teams[t].fwd[k][0] = -1;
            close(teams[t].fwd[k][1]); teams[t].fwd[k][1] = -1;
            close(teams[t].bwd[k][0]); teams[t].bwd[k][0] = -1;
            close(teams[t].bwd[k][1]); teams[t].bwd[k][1] = -1;
        }
    }

    /* ---------------- main round loop ---------------- */
    int round_id = 1;
    int *serials = calloc((size_t)cfg.furniture_piece_count, sizeof(int));
    if (!serials) { teardown(); return 2; }
    unsigned ref_rng = seed;

    while (!g_flag_term &&
           teams[0].wins < cfg.target_round_wins &&
           teams[1].wins < cfg.target_round_wins)
    {
        /* Generate this round's serials. */
        if (cfg.random_serials_enabled) {
            generate_unique_serials(serials, cfg.furniture_piece_count,
                                    &ref_rng, cfg.openmp_enabled);
        } else {
            for (int i = 0; i < cfg.furniture_piece_count; i++)
                serials[i] = cfg.fixed_serials[i];
        }
        fprintf(stderr, "[ref] === round %d starting (target wins=%d, score %d-%d) ===\n",
                round_id, cfg.target_round_wins, teams[0].wins, teams[1].wins);
        if (cfg.log_verbose) {
            fprintf(stderr, "[ref] serials: ");
            for (int i = 0; i < cfg.furniture_piece_count; i++)
                fprintf(stderr, "%d%s", serials[i],
                        i + 1 == cfg.furniture_piece_count ? "\n" : ",");
        }

        /* Send INIT to each team's source. */
        for (int t = 0; t < NUM_TEAMS; t++) {
            send_init_to_member(init_pipe[t][1], round_id, serials,
                                cfg.furniture_piece_count);
        }
        /* Wait for both sources to ACK init. */
        for (int t = 0; t < NUM_TEAMS; t++) {
            if (wait_for_ack(ack_pipe[t][0], MSG_INIT, 5000) < 0) {
                fprintf(stderr, "[ref] team %d INIT ack timed out\n", t);
                if (g_flag_term) break;
            }
        }

        /* Wait for an EVT_WIN on the FIFO. */
        int winner = -1;
        while (winner < 0 && !g_flag_term) {
            struct pollfd pfd = { .fd = fifo_fd_r, .events = POLLIN };
            int pr = poll(&pfd, 1, 1000);
            if (pr < 0) { if (errno == EINTR) continue; break; }
            if (pr == 0) continue;
            Event ev;
            if (evt_read(fifo_fd_r, &ev) < 0) break;
            if (ev.kind == EVT_WIN && ev.round_id == round_id) {
                winner = ev.team_id;
            }
        }
        if (winner < 0) break;

        teams[winner].wins++;
        fprintf(stderr, "[ref] >>> Team %d wins round %d  (score %d-%d) <<<\n",
                winner, round_id, teams[0].wins, teams[1].wins);

        if (teams[winner].wins >= cfg.target_round_wins) break;

        /* End-of-round reset: SIGUSR1 + MSG_RESET via init pipe. */
        for (int t = 0; t < NUM_TEAMS; t++) {
            for (int i = 0; i < teams[t].N; i++) {
                if (teams[t].pids[i] > 0) kill(teams[t].pids[i], SIGUSR1);
            }
            Message reset = { .type = MSG_RESET, .round_id = round_id };
            msg_write(init_pipe[t][1], &reset);
        }
        /* Wait for source ACKs. */
        for (int t = 0; t < NUM_TEAMS; t++) {
            wait_for_ack(ack_pipe[t][0], MSG_RESET, 3000);
        }
        /* Drain any stale events from previous round. */
        for (;;) {
            struct pollfd pfd = { .fd = fifo_fd_r, .events = POLLIN };
            int pr = poll(&pfd, 1, 50);
            if (pr <= 0) break;
            Event ev; if (evt_read(fifo_fd_r, &ev) < 0) break;
            (void)ev;
        }
        round_id++;
    }

    int champion = (teams[0].wins > teams[1].wins) ? 0 :
                   (teams[1].wins > teams[0].wins) ? 1 : -1;
    if (champion >= 0) {
        printf("\n=== Team %d wins the competition!  Final score %d-%d ===\n",
               champion, teams[0].wins, teams[1].wins);
    } else {
        printf("\n=== Competition ended without a champion (score %d-%d) ===\n",
               teams[0].wins, teams[1].wins);
    }

    free(serials);
    teardown();
    config_free(&cfg);
    return 0;
}
