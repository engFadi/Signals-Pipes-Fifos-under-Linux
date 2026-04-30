/* graphics.c — Phase 12 stub. Returns -1 to indicate "not built". */
#include "graphics.h"
#include <stdio.h>

int graphics_spawn_viewer(const Config *cfg) {
    if (cfg->opengl_enabled) {
        fprintf(stderr, "[gfx] OpenGL viewer requested but not yet implemented "
                        "(Phase 12). Continuing headless.\n");
    }
    return -1;
}
