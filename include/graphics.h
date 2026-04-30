/* graphics.h — OpenGL viewer process (Phase 12 stub). */
#ifndef FURNISH_GRAPHICS_H
#define FURNISH_GRAPHICS_H

#include "config.h"

/* Spawn the viewer.  In Phase 1 this is a no-op stub returning -1 so the
 * referee falls back to headless. */
int graphics_spawn_viewer(const Config *cfg);

#endif
