#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "pipeline_shared.h"

furniture_piece *pipeline_map_shared_furniture(furniture_piece *source, int count) {
    furniture_piece *shared = mmap(NULL,
                                   (size_t)count * sizeof(*shared),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS,
                                   -1,
                                   0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    memcpy(shared, source, (size_t)count * sizeof(*shared));
    return shared;
}

void pipeline_unmap_shared_furniture(furniture_piece *shared, furniture_piece *original, int count) {
    memcpy(original, shared, (size_t)count * sizeof(*shared));
    munmap(shared, (size_t)count * sizeof(*shared));
}

int pipeline_find_piece_index(furniture_piece *furniture, int count, int serial) {
    for (int i = 0; i < count; ++i) {
        if (furniture[i].serial_no == serial) {
            return i;
        }
    }
    return -1;
}
