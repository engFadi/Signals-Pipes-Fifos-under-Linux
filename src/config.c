/* config.c — small key=value config parser. */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static char *strip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static int parse_int_list(const char *s, int **out, int *count) {
    /* Counts commas first, then parses. Empty string => 0 elements. */
    int n = 0;
    if (*s) {
        n = 1;
        for (const char *p = s; *p; p++) if (*p == ',') n++;
    }
    *count = n;
    if (n == 0) { *out = NULL; return 0; }
    int *arr = calloc((size_t)n, sizeof(int));
    if (!arr) return -1;
    char *dup = strdup(s);
    if (!dup) { free(arr); return -1; }
    int i = 0;
    char *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        arr[i++] = atoi(strip(tok));
    }
    free(dup);
    *out = arr;
    return 0;
}

static void set_defaults(Config *c) {
    memset(c, 0, sizeof(*c));
    c->team_member_count      = 5;
    c->furniture_piece_count  = 10;
    c->target_round_wins      = 2;
    c->min_delay_ms           = 50;
    c->max_delay_ms           = 200;
    c->max_delay_cap_ms       = 1500;
    c->tiredness_increment_ms = 2;
    c->random_serials_enabled = 1;
    c->fixed_serials          = NULL;
    c->opengl_enabled         = 0;
    c->openmp_enabled         = 1;
    c->random_seed            = 0;
    c->log_verbose            = 1;
    snprintf(c->referee_fifo_path, sizeof(c->referee_fifo_path),
             "/tmp/furnish_referee.fifo");
}

int config_load(const char *path, Config *out) {
    set_defaults(out);

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "config: cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    char *serials_raw = NULL;
    char line[1024];
    int  lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *s = strip(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "config: line %d: missing '='\n", lineno);
            fclose(f); free(serials_raw); return -1;
        }
        *eq = '\0';
        char *key = strip(s);
        char *val = strip(eq + 1);
        /* Strip trailing inline comment from value. */
        char *hash = strchr(val, '#');
        if (hash) { *hash = '\0'; val = strip(val); }

        if      (!strcmp(key, "team_member_count"))      out->team_member_count      = atoi(val);
        else if (!strcmp(key, "furniture_piece_count"))  out->furniture_piece_count  = atoi(val);
        else if (!strcmp(key, "target_round_wins"))      out->target_round_wins      = atoi(val);
        else if (!strcmp(key, "min_delay_ms"))           out->min_delay_ms           = atoi(val);
        else if (!strcmp(key, "max_delay_ms"))           out->max_delay_ms           = atoi(val);
        else if (!strcmp(key, "max_delay_cap_ms"))       out->max_delay_cap_ms       = atoi(val);
        else if (!strcmp(key, "tiredness_increment_ms")) out->tiredness_increment_ms = atoi(val);
        else if (!strcmp(key, "random_serials_enabled")) out->random_serials_enabled = atoi(val);
        else if (!strcmp(key, "serials_list"))           { free(serials_raw); serials_raw = strdup(val); }
        else if (!strcmp(key, "opengl_enabled"))         out->opengl_enabled         = atoi(val);
        else if (!strcmp(key, "openmp_enabled"))         out->openmp_enabled         = atoi(val);
        else if (!strcmp(key, "random_seed"))            out->random_seed            = (unsigned)strtoul(val, NULL, 10);
        else if (!strcmp(key, "log_verbose"))            out->log_verbose            = atoi(val);
        else if (!strcmp(key, "referee_fifo_path")) {
            strncpy(out->referee_fifo_path, val, sizeof(out->referee_fifo_path) - 1);
            out->referee_fifo_path[sizeof(out->referee_fifo_path) - 1] = '\0';
        }
        else {
            fprintf(stderr, "config: line %d: unknown key '%s'\n", lineno, key);
        }
    }
    fclose(f);

    /* Parse serials list if needed. */
    if (!out->random_serials_enabled) {
        int n = 0;
        if (!serials_raw || !*serials_raw) {
            fprintf(stderr, "config: random_serials_enabled=0 requires serials_list\n");
            free(serials_raw); return -1;
        }
        if (parse_int_list(serials_raw, &out->fixed_serials, &n) < 0) {
            fprintf(stderr, "config: failed to parse serials_list\n");
            free(serials_raw); return -1;
        }
        if (n != out->furniture_piece_count) {
            fprintf(stderr, "config: serials_list has %d entries, expected %d\n",
                    n, out->furniture_piece_count);
            free(serials_raw); return -1;
        }
    }
    free(serials_raw);

    /* Validation. */
    if (out->team_member_count < 2) {
        fprintf(stderr, "config: team_member_count must be >= 2\n");      return -1;
    }
    if (out->furniture_piece_count < 1) {
        fprintf(stderr, "config: furniture_piece_count must be >= 1\n");  return -1;
    }
    if (out->target_round_wins < 1) {
        fprintf(stderr, "config: target_round_wins must be >= 1\n");      return -1;
    }
    if (out->min_delay_ms < 0 || out->max_delay_ms < out->min_delay_ms) {
        fprintf(stderr, "config: invalid delay range\n");                 return -1;
    }
    if (out->max_delay_cap_ms < out->max_delay_ms) {
        out->max_delay_cap_ms = out->max_delay_ms * 8;
    }
    return 0;
}

void config_free(Config *c) {
    if (!c) return;
    free(c->fixed_serials);
    c->fixed_serials = NULL;
}

void config_print(const Config *c) {
    fprintf(stderr,
        "[cfg] N=%d M=%d wins=%d delay=[%d..%d] cap=%d tired+=%d "
        "rand_serials=%d openmp=%d opengl=%d seed=%u verbose=%d\n",
        c->team_member_count, c->furniture_piece_count, c->target_round_wins,
        c->min_delay_ms, c->max_delay_ms, c->max_delay_cap_ms,
        c->tiredness_increment_ms, c->random_serials_enabled,
        c->openmp_enabled, c->opengl_enabled, c->random_seed, c->log_verbose);
}
