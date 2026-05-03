#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "constants.h"

/* Generic config value reader: returns value if found, otherwise returns default */
static int read_config_value(const char *path, const char *key, int default_val) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return default_val;
    }

    char line[LINE_BUF];
    int result = default_val;

    while (fgets(line, sizeof(line), file)) {
        char config_key[KEY_BUF];
        int value;

        if (sscanf(line, " %127[^=]=%d", config_key, &value) == 2) {
            /* trim trailing whitespace from key */
            for (char *p = config_key; *p; ++p) {
                if (*p == ' ' || *p == '\t') {
                    *p = '\0';
                    break;
                }
            }

            if (strcmp(config_key, key) == 0) {
                result = value;
                break;
            }
        }
    }

    fclose(file);
    return result;
}

static double read_config_double_value(const char *path, const char *key, double default_val) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return default_val;
    }

    char line[LINE_BUF];
    double result = default_val;

    while (fgets(line, sizeof(line), file)) {
        char config_key[KEY_BUF];
        double value;

        if (sscanf(line, " %127[^=]=%lf", config_key, &value) == 2) {
            for (char *p = config_key; *p; ++p) {
                if (*p == ' ' || *p == '\t') {
                    *p = '\0';
                    break;
                }
            }

            if (strcmp(config_key, key) == 0) {
                result = value;
                break;
            }
        }
    }

    fclose(file);
    return result;
}

int load_settings(int argc, char *argv[], AppSettings *settings) {
    const char *config_path = getenv("CONFIG_PATH");
    int override_count = 0;

    settings->config_path = CONFIG_DEFAULT_PATH;
    settings->child_count = MIN_CHILDREN;
    settings->furniture_pieces = DEFAULT_FURNITURE_PIECES;  /* default */
    settings->auto_serial = 1;         /* default: auto-assign */
    settings->min_pause = DEFAULT_MIN_PAUSE;
    settings->max_pause = DEFAULT_MAX_PAUSE;
    settings->win_rounds = DEFAULT_WIN_ROUNDS;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Usage: %s [-c config-path] [child-count]\n", argv[0]);
                return EXIT_FAILURE;
            }
            config_path = argv[++i];
            continue;
        }

        char *end = NULL;
        long value = strtol(argv[i], &end, 10);
        if (end == argv[i] || *end != '\0' || value <= 0) {
            fprintf(stderr, "Unknown argument: %s\nUsage: %s [-c config-path] [child-count]\n", argv[i], argv[0]);
            return EXIT_FAILURE;
        }

        override_count = (int)value;
    }

    if (config_path != NULL) {
        settings->config_path = config_path;
    }

    /* read members_no (child_count) */
    int config_count = read_config_value(settings->config_path, "members_no", -1);
    if (config_count >= MIN_CHILDREN) {
        settings->child_count = config_count;
    } else if (override_count == 0) {
        fprintf(stderr, "%s: invalid members_no — default %d used\n",
                settings->config_path, MIN_CHILDREN);
    }

    /* read furniture_pieces */
    int furniture_count = read_config_value(settings->config_path, "furniture_pieces", -1);
    if (furniture_count > 0) {
        settings->furniture_pieces = furniture_count;
    } else {
        fprintf(stderr, "%s: invalid furniture_pieces — default %d used\n",
                settings->config_path, DEFAULT_FURNITURE_PIECES);
        settings->furniture_pieces = DEFAULT_FURNITURE_PIECES;
    }

    /* read auto_serial */
    int auto_serial = read_config_value(settings->config_path, "auto-serial", -1);
    if (auto_serial == 0 || auto_serial == 1) {
        settings->auto_serial = auto_serial;
    } else {
        fprintf(stderr, "%s: invalid auto-serial — default %d used\n",
                settings->config_path, settings->auto_serial);
    }

    /* read min_pause */
    double min_pause = read_config_double_value(settings->config_path, "min_pause", DEFAULT_MIN_PAUSE);
    if (min_pause > 0.0) {
        settings->min_pause = min_pause;
    } else {
        fprintf(stderr, "%s: invalid min_pause — default %.2f used\n",
                settings->config_path, DEFAULT_MIN_PAUSE);
    }

    /* read max_pause */
    double max_pause = read_config_double_value(settings->config_path, "max_pause", DEFAULT_MAX_PAUSE);
    if (max_pause > 0.0) {
        settings->max_pause = max_pause;
    } else {
        fprintf(stderr, "%s: invalid max_pause — default %.2f used\n",
                settings->config_path, DEFAULT_MAX_PAUSE);
    }

    /* read win_rounds */
    int win_rounds = read_config_value(settings->config_path, "win_rounds", -1);
    if (win_rounds > 0) {
        settings->win_rounds = win_rounds;
    } else {
        fprintf(stderr, "%s: invalid win_rounds — default %d used\n",
                settings->config_path, DEFAULT_WIN_ROUNDS);
        settings->win_rounds = DEFAULT_WIN_ROUNDS;
    }

    if (settings->min_pause > settings->max_pause) {
        double tmp = settings->min_pause;
        settings->min_pause = settings->max_pause;
        settings->max_pause = tmp;
    }

    if (override_count > 0) {
        if (override_count >= MIN_CHILDREN) {
            settings->child_count = override_count;
        } else {
            fprintf(stderr, "invalid child-count %d — default %d used\n",
                    override_count, MIN_CHILDREN);
            settings->child_count = MIN_CHILDREN;
        }
    }

    return EXIT_SUCCESS;
}

