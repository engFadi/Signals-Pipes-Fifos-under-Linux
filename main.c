#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "pipeline.h"
#include "furniture.h"

int main(int argc, char *argv[]){
    AppSettings settings;

    if (load_settings(argc, argv, &settings) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    /* Initialize furniture */
    furniture_piece *furniture = furniture_init(settings.furniture_pieces, settings.auto_serial);
    if (furniture == NULL) {
        fprintf(stderr, "Failed to initialize furniture\n");
        return EXIT_FAILURE;
    }

    /* Display furniture inventory table */
    furniture_display_table(furniture, settings.furniture_pieces);

    int result = run_pipeline(settings.child_count, furniture, settings.furniture_pieces);

    furniture_free(furniture);
    return result;
}