

struct VisualShm {
    int furniture_count;
    int pipeline_len;
    int win_rounds;
    int round;
    int team1_wins;
    int team2_wins;
    int champion;
    int event_head[2];
    int event_count[2];
    int event_lock[2];



    struct {
        int piece_index;
        int from_node;
        int to_node;
        piece_status status;
        float duration;
    } events[2][VISUAL_EVENT_CAP];
};

static void visual_event_lock(VisualShm *v, int team_index){
    while(__sync_lock_test_and_set(&v->event_lock[team_index], 1)){
        usleep(1000);
    }
}

static void visual_event_unlock(VisualShm *v, int team_index){
    __sync_lock_release(&v->event_lock[team_index]);
}

static furniture_piece *shm_t1(VisualShm *v){
    return (furniture_piece *)((char *)v + sizeof(VisualShm));
}
static furniture_piece *shm_t2(VisualShm *v){
    return shm_t1(v) + v->furniture_count;
}
static size_t shm_sz(int fc){
    return sizeof(VisualShm) + 2*(size_t)fc*sizeof(furniture_piece);
}




VisualShm *visual_shm_create(int fc){
    VisualShm *v = mmap(NULL, shm_sz(fc),
                        PROT_READ|PROT_WRITE,
                        MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if(v == MAP_FAILED){ perror("visual mmap"); return NULL; }
    memset(v, 0, shm_sz(fc));
    v->furniture_count = fc;
    return v;
}
void visual_destroy(VisualShm *v, int fc){
    if(v) munmap(v, shm_sz(fc));
}
void visual_update(VisualShm *v,
                   furniture_piece *t1, furniture_piece *t2,
                   int fc, int round,
                   int t1w, int t2w, int win_rounds){
    if(!v) return;
    v->round      = round;
    v->team1_wins = t1w;
    v->team2_wins = t2w;
    v->win_rounds = win_rounds;
    v->event_head[0] = v->event_head[1] = 0;
    v->event_count[0] = v->event_count[1] = 0;
    v->event_lock[0] = v->event_lock[1] = 0;
    memcpy(shm_t1(v), t1, (size_t)fc*sizeof(furniture_piece));
    memcpy(shm_t2(v), t2, (size_t)fc*sizeof(furniture_piece));
}
void visual_set_champion(VisualShm *v, int champ){
    if(v) v->champion = champ;
}

furniture_piece *visual_team_furniture(VisualShm *v, int team){
    if(!v) return NULL;
    return (team == 2) ? shm_t2(v) : shm_t1(v);
}

void visual_queue_hop(VisualShm *v,
                      int team,
                      int piece_index,
                      int from_node,
                      int to_node,
                      piece_status status,
                      double duration_seconds){
    if(!v || team < 1 || team > 2)
        return;
    if(piece_index < 0 || piece_index >= v->furniture_count)
        return;
    if(from_node < 0 || to_node < 0 ||
       from_node >= v->pipeline_len || to_node >= v->pipeline_len)
        return;

    int t = team - 1;
    visual_event_lock(v, t);
    if(v->event_count[t] >= VISUAL_EVENT_CAP){
        visual_event_unlock(v, t);
        return;
    }

    int tail = (v->event_head[t] + v->event_count[t]) % VISUAL_EVENT_CAP;
    v->events[t][tail].piece_index = piece_index;
    v->events[t][tail].from_node = from_node;
    v->events[t][tail].to_node = to_node;
    v->events[t][tail].status = status;
    v->events[t][tail].duration = (float)duration_seconds;
    v->event_count[t]++;
    visual_event_unlock(v, t);
}


