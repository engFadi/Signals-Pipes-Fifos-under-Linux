
#define TIMER_MS      16     
#define TRAVEL_TICKS  16     
#define PAUSE_TICKS    2     
#define MAX_PIECES   2048
#define ANIM_NONE       0
#define TAU           6.28318530f
#define PI            3.14159265f




typedef struct {
    int          active;
    int          serial_no;
    int          order;
    piece_status final_status;
    piece_status last_shm_status;
    piece_status status;      

    int          cur_node;    
    int          prev_node;   
    int          dest_node;   
    int          traveling;   
    int          direction;
    int          route;
    float        progress;    
    float        progress_step;
    int          pause_left;  
    float        bob_phase;   
} AnimPiece;

typedef struct {
    int piece;
    int from_node;
    int to_node;
    piece_status status;
    float duration;
} AnimEvent;

typedef struct {
    AnimEvent queue[VISUAL_EVENT_CAP];
    int head;
    int count;
} TeamAnim;

static AnimPiece  g_anim[2][MAX_PIECES];
static TeamAnim   g_team_anim[2];
static int        g_fc   = 0;
static int        g_plen = 0;
static float      g_clock = 0.0f;
static VisualShm *g_vshm  = NULL;
static int        g_W = 1140, g_H = 720;
static int        g_seen_round = -1;


static float ease(float t){ return t*t*(3.0f - 2.0f*t); }


static void visual_piece_tick(AnimPiece *a, furniture_piece *sp){
    a->serial_no = sp->serial_no;
    a->order = sp->order;
    a->bob_phase += 0.07f;

    if(a->traveling){
        a->progress += a->progress_step;
        if(a->progress >= 1.0f){
            a->progress = 1.0f;
            a->cur_node = a->dest_node;
            a->traveling = 0;
            a->pause_left = PAUSE_TICKS;
        }
        return;
    }

    if(a->pause_left > 0){
        a->pause_left--;
        return;
    }


}

static void visual_piece_init(AnimPiece *a, furniture_piece *sp, int idx){
    a->active = 1;
    a->serial_no = sp->serial_no;
    a->order = sp->order;
    a->status = AVAILABLE;
    a->final_status = AVAILABLE;
    a->last_shm_status = AVAILABLE;
    a->cur_node = 0;
    a->prev_node = 0;
    a->dest_node = 0;
    a->traveling = 0;
    a->direction = 0;
    a->route = 0;
    a->progress = 0.0f;
    a->progress_step = 1.0f / (float)TRAVEL_TICKS;
    a->pause_left = 0;
    a->bob_phase = (float)idx * 1.1f;
}

static void team_anim_reset(int team){
    g_team_anim[team].head = 0;
    g_team_anim[team].count = 0;
}

static void team_anim_queue_event(int team, int piece, int from_node, int to_node, piece_status status, float duration){
    TeamAnim *ta = &g_team_anim[team];

    if(piece < 0 || piece >= MAX_PIECES)
        return;
    if(from_node < 0 || to_node < 0 || from_node >= g_plen || to_node >= g_plen)
        return;

    if(ta->count >= VISUAL_EVENT_CAP)
        return;

    int tail = (ta->head + ta->count) % VISUAL_EVENT_CAP;
    ta->queue[tail].piece = piece;
    ta->queue[tail].from_node = from_node;
    ta->queue[tail].to_node = to_node;
    ta->queue[tail].status = status;
    ta->queue[tail].duration = duration;
    ta->count++;
}

static int team_anim_remove_event(int team, int offset, AnimEvent *ev){
    TeamAnim *ta = &g_team_anim[team];
    if(ta->count <= 0)
        return 0;

    int pos = (ta->head + offset) % VISUAL_EVENT_CAP;
    *ev = ta->queue[pos];

    for(int i = offset; i < ta->count - 1; i++){
        int dst = (ta->head + i) % VISUAL_EVENT_CAP;
        int src = (ta->head + i + 1) % VISUAL_EVENT_CAP;
        ta->queue[dst] = ta->queue[src];
    }

    ta->count--;
    return 1;
}

static piece_status visual_final_status(piece_status shm_status, piece_status queued_status){
    if(shm_status == PLACED || shm_status == BLOCKED || shm_status == AVAILABLE)
        return shm_status;
    return queued_status;
}

static void team_anim_scan_shared(int team, furniture_piece *shm){
    for(int p = 0; p < g_fc && p < MAX_PIECES; p++){
        AnimPiece *a = &g_anim[team][p];
        piece_status st = shm[p].status;

        if(!a->active)
            visual_piece_init(a, &shm[p], p);

        a->serial_no = shm[p].serial_no;
        a->order = shm[p].order;

        if(st != a->last_shm_status){
            a->last_shm_status = st;
        }
    }
}

static void team_anim_drain_shared_events(int team){
    int t = team;
    if(!g_vshm)
        return;

    visual_event_lock(g_vshm, t);
    while(g_vshm->event_count[t] > 0){
        int pos = g_vshm->event_head[t];
        AnimEvent ev;
        ev.piece = g_vshm->events[t][pos].piece_index;
        ev.from_node = g_vshm->events[t][pos].from_node;
        ev.to_node = g_vshm->events[t][pos].to_node;
        ev.status = g_vshm->events[t][pos].status;
        ev.duration = g_vshm->events[t][pos].duration;

        g_vshm->event_head[t] = (g_vshm->event_head[t] + 1) % VISUAL_EVENT_CAP;
        g_vshm->event_count[t]--;

        team_anim_queue_event(team, ev.piece, ev.from_node, ev.to_node, ev.status, ev.duration);
    }
    visual_event_unlock(g_vshm, t);
}

static int visual_piece_busy(const AnimPiece *a){
    return a->traveling || a->pause_left > 0;
}

static float visual_progress_step(float duration){
    float ticks = duration * (1000.0f / (float)TIMER_MS);
    if(ticks < 1.0f)
        ticks = 1.0f;
    return 1.0f / ticks;
}

static void team_anim_start_ready(int team){
    TeamAnim *ta = &g_team_anim[team];
    int offset = 0;

    while(offset < ta->count){
        AnimEvent ev = ta->queue[(ta->head + offset) % VISUAL_EVENT_CAP];

        if(ev.piece < 0 || ev.piece >= g_fc || ev.piece >= MAX_PIECES){
            team_anim_remove_event(team, offset, &ev);
            continue;
        }

        AnimPiece *a = &g_anim[team][ev.piece];
        if(visual_piece_busy(a)){
            offset++;
            continue;
        }

        team_anim_remove_event(team, offset, &ev);
        a->cur_node = ev.from_node;
        a->prev_node = ev.from_node;
        a->dest_node = ev.to_node;
        a->traveling = 1;
        a->progress = 0.0f;
        a->progress_step = visual_progress_step(ev.duration);
        a->pause_left = 0;
        a->route = ANIM_NONE;
        a->direction = (ev.to_node > ev.from_node) ? 1 : -1;
        a->status = ev.status;
        a->final_status = ev.status;
    }
}

static void team_anim_tick_active(int team, furniture_piece *shm){
    for(int p = 0; p < g_fc && p < MAX_PIECES; p++){
        AnimPiece *a = &g_anim[team][p];
        if(!a->active || !visual_piece_busy(a))
            continue;

        visual_piece_tick(a, &shm[p]);

        if(!visual_piece_busy(a)){
            a->status = visual_final_status(shm[p].status, a->final_status);
            a->final_status = a->status;
            a->direction = 0;
        }
    }
}

static void anim_tick_team(int team, furniture_piece *shm){
    team_anim_scan_shared(team, shm);
    team_anim_drain_shared_events(team);
    team_anim_start_ready(team);
    team_anim_tick_active(team, shm);
    team_anim_start_ready(team);
}



