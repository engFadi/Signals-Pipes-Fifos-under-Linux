/* team.h — per-member role functions executed by child processes. */
#ifndef FURNISH_TEAM_H
#define FURNISH_TEAM_H

#include "config.h"

/* Each child is given the four fds it needs:
 *   fwd_in   : read end of pipe coming from previous member  (or -1 if source)
 *   fwd_out  : write end of pipe going to next member        (or -1 if sink)
 *   bwd_in   : read end of pipe coming from next member      (or -1 if sink)
 *   bwd_out  : write end of pipe going to previous member    (or -1 if source)
 *   referee_fifo_w : write end of the referee FIFO           (sink only; -1 otherwise)
 */
typedef struct {
    int member_index;          /* 0..N-1 */
    int team_id;               /* 0 or 1 */
    int N;                     /* team size */
    int fwd_in, fwd_out;
    int bwd_in, bwd_out;
    int referee_fifo_w;        /* sinks only */
    const Config *cfg;
    unsigned rng_state;        /* per-process RNG state */
} MemberCtx;

/* Returns process exit code. */
int team_run_source(MemberCtx *ctx);
int team_run_middle(MemberCtx *ctx);
int team_run_sink  (MemberCtx *ctx);

/* Compute a tired delay in milliseconds.
 * count = number of pieces this member has handled so far. */
int tired_delay_ms(const Config *cfg, unsigned *rng, long count);

/* sleep helper that retries on EINTR. */
void sleep_ms(int ms);

#endif /* FURNISH_TEAM_H */
