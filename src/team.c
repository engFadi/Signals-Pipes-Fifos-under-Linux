/* team.c — source / middle / sink role loops. */
#include "team.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

extern volatile sig_atomic_t g_flag_term;
extern volatile sig_atomic_t g_flag_reset;

void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {
        if (g_flag_term || g_flag_reset) return;
    }
}

int tired_delay_ms(const Config *cfg, unsigned *rng, long count) {
    int base = rand_range(rng, cfg->min_delay_ms, cfg->max_delay_ms);
    long extra = (long)cfg->tiredness_increment_ms * count;
    long total = (long)base + extra;
    if (total > cfg->max_delay_cap_ms) total = cfg->max_delay_cap_ms;
    if (total < cfg->min_delay_ms)      total = cfg->min_delay_ms;
    return (int)total;
}

static const char *role_str(int idx, int N) {
    if (idx == 0) return "SRC";
    if (idx == N - 1) return "SNK";
    return "MID";
}

#define VLOG(ctx, ...) do { \
    if ((ctx)->cfg->log_verbose) { \
        fprintf(stderr, "[T%d %s#%d pid=%d] ", \
            (ctx)->team_id, role_str((ctx)->member_index, (ctx)->N), \
            (ctx)->member_index, (int)getpid()); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while (0)

/* ------------------------------------------------------------------------ */
/* Source — member 0                                                         */
/* ------------------------------------------------------------------------ */

int team_run_source(MemberCtx *ctx) {
    int M = ctx->cfg->furniture_piece_count;
    int *pile = calloc((size_t)M, sizeof(int));   /* current serials owned */
    int *in_pile = calloc((size_t)M + 1, sizeof(int)); /* indexed by serial */
    if (!pile || !in_pile) return 1;

    long handled = 0;
    int  current_round = 0;
    int  blocked_piece = -1;
    int  pile_size = 0;

    /* helper: load round state from an MSG_INIT followed by M MSG_PIECE INITs. */
    /* Protocol used here: referee sends one MSG_INIT (payload=M, round_id=R)
     * then M MSG_PIECE messages with piece_id set to each serial; status=OK,
     * direction=FWD acts as the "INIT delivery" stream. */
    /* For simplicity we cheat: referee writes M+1 messages on fwd_in. */
    while (!g_flag_term) {
        Message m;
        if (msg_read(ctx->fwd_in, &m) < 0) {
            if (g_flag_term) break;
            VLOG(ctx, "fwd_in read failed: %s\n", strerror(errno));
            break;
        }
        if (m.type == MSG_TERMINATE) break;
        if (m.type == MSG_INIT) {
            current_round = m.round_id;
            int count = m.payload;
            pile_size = 0;
            memset(in_pile, 0, sizeof(int) * (size_t)(M + 1));
            /* Forward the INIT header to the next member so the chain (incl. sink)
             * learns the expected order. */
            msg_write(ctx->fwd_out, &m);
            for (int i = 0; i < count; i++) {
                Message ser;
                if (msg_read(ctx->fwd_in, &ser) < 0) { free(pile); free(in_pile); return 1; }
                pile[pile_size++] = ser.piece_id;
                if (ser.piece_id >= 1 && ser.piece_id <= M) in_pile[ser.piece_id] = 1;
                msg_write(ctx->fwd_out, &ser);   /* propagate to chain */
            }
            blocked_piece = -1;
            VLOG(ctx, "round %d initialised with %d pieces\n", current_round, pile_size);
            /* ACK back so referee knows source is ready. */
            Message ack = { .type = MSG_INIT, .round_id = current_round, .team_id = ctx->team_id };
            msg_write(ctx->bwd_out, &ack);
            break; /* fall into work loop */
        }
    }

    while (!g_flag_term) {
        if (g_flag_reset) {
            g_flag_reset = 0;
            VLOG(ctx, "reset signal received, awaiting MSG_RESET drain\n");
        }

        /* Drain any backward traffic first. */
        struct pollfd pfd[2];
        pfd[0].fd = ctx->bwd_in;  pfd[0].events = POLLIN;
        pfd[1].fd = ctx->fwd_in;  pfd[1].events = POLLIN;
        int pr = poll(pfd, 2, 0);
        if (pr > 0) {
            if (pfd[0].revents & POLLIN) {
                Message rmsg;
                if (msg_read(ctx->bwd_in, &rmsg) < 0) break;
                /* Drop stale messages from a previous round. */
                if ((rmsg.type == MSG_PIECE || rmsg.type == MSG_ACK_OK) &&
                    rmsg.round_id != current_round) {
                    VLOG(ctx, "drop stale bwd msg type=%d pid=%d r=%d\n",
                         rmsg.type, rmsg.piece_id, rmsg.round_id);
                } else if (rmsg.type == MSG_PIECE && rmsg.status == STAT_REJECTED) {
                    blocked_piece = rmsg.piece_id;
                    VLOG(ctx, "piece %d REJECTED, blocked\n", rmsg.piece_id);
                } else if (rmsg.type == MSG_ACK_OK) {
                    /* Remove accepted piece from pile. */
                    int pid = rmsg.piece_id;
                    if (pid >= 1 && pid <= M && in_pile[pid]) {
                        in_pile[pid] = 0;
                        for (int i = 0; i < pile_size; i++) {
                            if (pile[i] == pid) {
                                pile[i] = pile[--pile_size];
                                break;
                            }
                        }
                    }
                    blocked_piece = -1;
                    VLOG(ctx, "piece %d ACCEPTED, pile=%d\n", pid, pile_size);
                }
            }
            if (pfd[1].revents & POLLIN) {
                Message ctl;
                if (msg_read(ctx->fwd_in, &ctl) < 0) break;
                if (ctl.type == MSG_RESET) {
                    /* Forward the RESET so the chain drains, then ACK. */
                    msg_write(ctx->fwd_out, &ctl);
                    VLOG(ctx, "MSG_RESET — pile=%d, awaiting next INIT\n", pile_size);
                    Message ack = { .type = MSG_RESET, .round_id = current_round, .team_id = ctx->team_id };
                    msg_write(ctx->bwd_out, &ack);
                    /* Block on next INIT. */
                    Message in;
                    if (msg_read(ctx->fwd_in, &in) < 0) break;
                    if (in.type == MSG_TERMINATE) {
                        msg_write(ctx->fwd_out, &in);
                        break;
                    }
                    if (in.type == MSG_INIT) {
                        current_round = in.round_id;
                        int count = in.payload;
                        pile_size = 0;
                        memset(in_pile, 0, sizeof(int) * (size_t)(M + 1));
                        msg_write(ctx->fwd_out, &in);   /* propagate */
                        for (int i = 0; i < count; i++) {
                            Message ser;
                            if (msg_read(ctx->fwd_in, &ser) < 0) { free(pile); free(in_pile); return 1; }
                            pile[pile_size++] = ser.piece_id;
                            in_pile[ser.piece_id] = 1;
                            msg_write(ctx->fwd_out, &ser);
                        }
                        blocked_piece = -1;
                        Message ack2 = { .type = MSG_INIT, .round_id = current_round, .team_id = ctx->team_id };
                        msg_write(ctx->bwd_out, &ack2);
                        VLOG(ctx, "round %d ready (%d pieces)\n", current_round, pile_size);
                    }
                } else if (ctl.type == MSG_TERMINATE) {
                    msg_write(ctx->fwd_out, &ctl);
                    break;
                }
            }
            continue;
        }

        if (pile_size == 0) { sleep_ms(5); continue; }

        /* Pick a random non-blocked piece. */
        int candidates = pile_size - (blocked_piece >= 0 && blocked_piece <= M && in_pile[blocked_piece] ? 1 : 0);
        if (candidates <= 0) { sleep_ms(5); continue; }
        int chosen = -1;
        for (int tries = 0; tries < 32; tries++) {
            int idx = rand_range(&ctx->rng_state, 0, pile_size - 1);
            if (pile[idx] != blocked_piece) { chosen = pile[idx]; break; }
        }
        if (chosen < 0) {
            /* Linear fallback. */
            for (int i = 0; i < pile_size; i++)
                if (pile[i] != blocked_piece) { chosen = pile[i]; break; }
            if (chosen < 0) { sleep_ms(5); continue; }
        }

        int delay = tired_delay_ms(ctx->cfg, &ctx->rng_state, handled);
        VLOG(ctx, "send piece %d (delay %d ms, pile=%d, blocked=%d)\n",
             chosen, delay, pile_size, blocked_piece);
        sleep_ms(delay);

        Message out = {
            .type = MSG_PIECE, .piece_id = chosen, .direction = DIR_FWD,
            .team_id = ctx->team_id, .round_id = current_round, .status = STAT_OK
        };
        if (msg_write(ctx->fwd_out, &out) < 0) {
            if (errno == EPIPE) break;
            VLOG(ctx, "fwd_out write failed: %s\n", strerror(errno));
            break;
        }
        handled++;
    }

    free(pile);
    free(in_pile);
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Middle — 0 < member_index < N-1                                           */
/* ------------------------------------------------------------------------ */

int team_run_middle(MemberCtx *ctx) {
    long handled = 0;
    while (!g_flag_term) {
        struct pollfd pfd[2];
        pfd[0].fd = ctx->fwd_in;  pfd[0].events = POLLIN;
        pfd[1].fd = ctx->bwd_in;  pfd[1].events = POLLIN;
        int pr = poll(pfd, 2, 1000);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pr == 0) continue;

        if (pfd[0].revents & POLLIN) {
            Message m;
            if (msg_read(ctx->fwd_in, &m) < 0) break;
            if (m.type == MSG_TERMINATE) {
                /* propagate then exit */
                msg_write(ctx->fwd_out, &m);
                break;
            }
            if (m.type == MSG_RESET || m.type == MSG_INIT) {
                msg_write(ctx->fwd_out, &m);   /* propagate untouched */
                continue;
            }
            int delay = tired_delay_ms(ctx->cfg, &ctx->rng_state, handled);
            VLOG(ctx, "fwd  piece %d (delay %d ms)\n", m.piece_id, delay);
            sleep_ms(delay);
            if (msg_write(ctx->fwd_out, &m) < 0) {
                if (errno == EPIPE) break;
            }
            handled++;
        }
        if (pfd[1].revents & POLLIN) {
            Message m;
            if (msg_read(ctx->bwd_in, &m) < 0) break;
            if (m.type == MSG_TERMINATE) {
                msg_write(ctx->bwd_out, &m);
                break;
            }
            if (m.type == MSG_RESET || m.type == MSG_INIT || m.type == MSG_ACK_OK) {
                msg_write(ctx->bwd_out, &m);
                continue;
            }
            int delay = tired_delay_ms(ctx->cfg, &ctx->rng_state, handled) / 2;
            VLOG(ctx, "bwd  piece %d (delay %d ms)\n", m.piece_id, delay);
            sleep_ms(delay);
            if (msg_write(ctx->bwd_out, &m) < 0) {
                if (errno == EPIPE) break;
            }
            handled++;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Sink — member_index == N-1                                                */
/* ------------------------------------------------------------------------ */

int team_run_sink(MemberCtx *ctx) {
    int  M = ctx->cfg->furniture_piece_count;
    int  expected_idx = 0;            /* index into the sink's own copy */
    int *expected_seq = calloc((size_t)M, sizeof(int));
    if (!expected_seq) return 1;
    int  accepted_count = 0;
    int  current_round = 0;
    long handled = 0;

    while (!g_flag_term) {
        Message m;
        if (msg_read(ctx->fwd_in, &m) < 0) {
            if (g_flag_term) break;
            VLOG(ctx, "fwd_in read failed: %s\n", strerror(errno));
            break;
        }
        if (m.type == MSG_TERMINATE) break;
        if (m.type == MSG_INIT) {
            /* Sink also receives the serial list so it knows the expected order. */
            current_round = m.round_id;
            int count = m.payload;
            for (int i = 0; i < count; i++) {
                Message ser;
                if (msg_read(ctx->fwd_in, &ser) < 0) { free(expected_seq); return 1; }
                expected_seq[i] = ser.piece_id;
            }
            expected_idx = 0;
            accepted_count = 0;
            VLOG(ctx, "round %d expecting %d pieces, first=%d\n",
                 current_round, count, count > 0 ? expected_seq[0] : -1);
            continue;
        }
        if (m.type == MSG_RESET) {
            /* Drain any further messages until next INIT. */
            VLOG(ctx, "MSG_RESET received\n");
            expected_idx = 0;
            accepted_count = 0;
            continue;
        }
        if (m.type == MSG_PIECE) {
            /* Drop stale pieces from a previous round, or any extra pieces
             * that arrive after the round has already been won. */
            if (m.round_id != current_round || accepted_count >= M) {
                VLOG(ctx, "drop stale fwd piece %d (r=%d)\n", m.piece_id, m.round_id);
                continue;
            }
            int delay = tired_delay_ms(ctx->cfg, &ctx->rng_state, handled);
            sleep_ms(delay);
            handled++;
            int expected_pid = expected_seq[expected_idx];
            if (m.piece_id == expected_pid) {
                accepted_count++;
                expected_idx++;
                VLOG(ctx, "ACCEPT %d (%d/%d)\n", m.piece_id, accepted_count, M);
                Message ack = {
                    .type = MSG_ACK_OK, .piece_id = m.piece_id,
                    .direction = DIR_BWD, .team_id = ctx->team_id,
                    .round_id = current_round, .status = STAT_OK
                };
                msg_write(ctx->bwd_out, &ack);
                if (accepted_count == M) {
                    Event ev = { .kind = EVT_WIN, .team_id = ctx->team_id,
                                 .round_id = current_round, .extra = accepted_count };
                    if (evt_write(ctx->referee_fifo_w, &ev) < 0) {
                        VLOG(ctx, "evt_write failed: %s\n", strerror(errno));
                    } else {
                        VLOG(ctx, "round %d WON, notified referee\n", current_round);
                    }
                }
            } else {
                VLOG(ctx, "REJECT %d (expected %d) — sending back\n",
                     m.piece_id, expected_pid);
                Message back = {
                    .type = MSG_PIECE, .piece_id = m.piece_id,
                    .direction = DIR_BWD, .team_id = ctx->team_id,
                    .round_id = current_round, .status = STAT_REJECTED
                };
                msg_write(ctx->bwd_out, &back);
            }
        }
    }
    free(expected_seq);
    return 0;
}
