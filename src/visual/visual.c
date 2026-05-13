#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "constants.h"
#include "visual.h"

#define VISUAL_EVENT_CAP 8192

#include "visual_shared.c"
#include "visual_animation.c"
#include "visual_draw.c"
#include "visual_glut.c"
