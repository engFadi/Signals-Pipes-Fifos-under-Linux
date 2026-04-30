/* ipc.h — message struct, pipe/FIFO helpers, signal handling. */
#ifndef FURNISH_IPC_H
#define FURNISH_IPC_H

#include <sys/types.h>
#include <signal.h>

typedef enum {
    MSG_PIECE      = 1, /* a furniture piece travelling */
    MSG_INIT       = 2, /* round init: source receives the new serial list */
    MSG_RESET      = 3, /* end-of-round drain marker */
    MSG_TERMINATE  = 4, /* graceful shutdown */
    MSG_ACK_OK     = 5  /* sink → source: piece accepted, unblock */
} MsgType;

typedef enum { DIR_FWD = 0, DIR_BWD = 1 } Direction;
typedef enum { STAT_OK = 0, STAT_REJECTED = 1 } Status;

typedef struct {
    int       type;       /* MsgType */
    int       piece_id;
    int       direction;  /* Direction */
    int       team_id;
    int       round_id;
    int       status;     /* Status */
    int       payload;    /* generic int slot (e.g. serial count for INIT) */
} Message;

/* Event written by sinks (and viewer) to the referee FIFO. */
typedef enum {
    EVT_WIN      = 1,
    EVT_PROGRESS = 2
} EventKind;

typedef struct {
    int kind;       /* EventKind */
    int team_id;
    int round_id;
    int extra;      /* piece_id or accepted_count */
} Event;

/* Robust read/write that retry on EINTR and complete a full message. */
int msg_write(int fd, const Message *m);
int msg_read (int fd, Message *m);
int evt_write(int fd, const Event *e);
int evt_read (int fd, Event *e);

/* Signal-safe self-pipe: sets up a non-blocking pipe and registers handlers
 * for SIGINT/SIGTERM/SIGUSR1/SIGUSR2/SIGCHLD/SIGPIPE.  All handlers do the
 * minimum: write a single byte (the signal number) to the self-pipe write
 * end and set the matching volatile flag. */
extern volatile sig_atomic_t g_flag_term;
extern volatile sig_atomic_t g_flag_reset;     /* SIGUSR1 = reset/abort round */
extern volatile sig_atomic_t g_flag_newround;  /* SIGUSR2 = new round started */
extern volatile sig_atomic_t g_flag_chld;

int  ipc_install_handlers(void);
int  ipc_self_pipe_read_fd(void);

/* Generate M unique serial numbers in [1..M] using Fisher–Yates.
 * If openmp_enabled, the fill loop is parallelised (the shuffle stays serial,
 * which is fine for correctness). */
void generate_unique_serials(int *out, int M, unsigned *rng_state, int openmp_enabled);

/* Thread-safe-ish rand_r wrapper. */
int  rand_range(unsigned *state, int lo, int hi);

#endif /* FURNISH_IPC_H */
