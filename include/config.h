/* config.h — read user-defined values from a text file. */
#ifndef FURNISH_CONFIG_H
#define FURNISH_CONFIG_H

#include <stddef.h>

typedef struct {
    int  team_member_count;        /* N (>= 2) */
    int  furniture_piece_count;    /* M (>= 1) */
    int  target_round_wins;
    int  min_delay_ms, max_delay_ms, max_delay_cap_ms;
    int  tiredness_increment_ms;
    int  random_serials_enabled;
    int *fixed_serials;            /* length M when random_serials_enabled==0; else NULL */
    int  opengl_enabled;
    int  openmp_enabled;
    unsigned random_seed;          /* 0 => seed from time() */
    int  log_verbose;
    char referee_fifo_path[256];
} Config;

/* Returns 0 on success, prints an error and returns -1 otherwise. */
int  config_load(const char *path, Config *out);
void config_free(Config *c);
void config_print(const Config *c);

#endif /* FURNISH_CONFIG_H */
