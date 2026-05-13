#ifndef SIGNALS_PIPES_VISUAL_H
#define SIGNALS_PIPES_VISUAL_H

#include "constants.h"


typedef struct VisualShm VisualShm;


VisualShm *visual_shm_create(int furniture_count);


void visual_destroy(VisualShm *v, int furniture_count);





void visual_launch(VisualShm *vshm,
                   int        furniture_count,
                   int        pipeline_len,
                   int        win_rounds);





void visual_update(VisualShm       *v,
                   furniture_piece *t1,
                   furniture_piece *t2,
                   int              furniture_count,
                   int              round,
                   int              team1_wins,
                   int              team2_wins,
                   int              win_rounds);


void visual_set_champion(VisualShm *v, int champion);





furniture_piece *visual_team_furniture(VisualShm *v, int team);


void visual_queue_hop(VisualShm *v,
                      int        team,
                      int        piece_index,
                      int        from_node,
                      int        to_node,
                      piece_status status,
                      double     duration_seconds);

#endif 
