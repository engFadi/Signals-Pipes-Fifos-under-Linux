#ifndef SIGNALS_PIPES_CONFIG_H
#define SIGNALS_PIPES_CONFIG_H

typedef struct {
    const char *config_path;
    int child_count;
    int furniture_pieces;
    int auto_serial;
    double min_pause;
    double max_pause;
} AppSettings;

int load_settings(int argc, char *argv[], AppSettings *settings);

#endif /* SIGNALS_PIPES_CONFIG_H */
