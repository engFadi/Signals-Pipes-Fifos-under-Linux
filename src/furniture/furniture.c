#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "furniture.h"
#include "constants.h"

/* Fisher-Yates shuffle for generating unique random serial numbers */
static void shuffle_serials(int *arr, int count) {
    srand((unsigned int)time(NULL) ^ getpid());
    for (int i = count - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* Check if serial number is already used */
static int is_serial_used(int *serials, int count, int serial) {
    for (int i = 0; i < count; ++i) {
        if (serials[i] == serial) {
            return 1;
        }
    }
    return 0;
}

furniture_piece *furniture_init(int count, int auto_serial) {
    furniture_piece *furniture = malloc((size_t)count * sizeof(furniture_piece));
    if (furniture == NULL) {
        perror("malloc");
        return NULL;
    }

    if (auto_serial == 1) {
        /* auto-assign unique random serial numbers */
        int *serials = malloc((size_t)count * sizeof(int));
        if (serials == NULL) {
            perror("malloc");
            free(furniture);
            return NULL;
        }

        /* initialize serials to 1..count */
        for (int i = 0; i < count; ++i) {
            serials[i] = i + 1;
        }

        /* shuffle */
        shuffle_serials(serials, count);

        /* assign to furniture */
        for (int i = 0; i < count; ++i) {
            furniture[i].serial_no = serials[i];
            furniture[i].order = i;
            furniture[i].status = AVAILABLE;
        }

        free(serials);
        printf("Auto-assigned %d unique serial numbers\n", count);
    } else {
        /* user input mode */
        int *serials = malloc((size_t)count * sizeof(int));
        if (serials == NULL) {
            perror("malloc");
            free(furniture);
            return NULL;
        }
        memset(serials, 0, (size_t)count * sizeof(int));

        printf("Enter serial number for each furniture piece (%d total):\n", count);
        for (int i = 0; i < count; ++i) {
            int serial = 0;
            int valid = 0;

            while (!valid) {
                printf("Piece %d serial number: ", i + 1);
                fflush(stdout);

                if (scanf("%d", &serial) != 1) {
                    /* clear input buffer */
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF)
                        ;
                    printf("Invalid input. Enter a positive integer.\n");
                    continue;
                }

                if (serial <= 0) {
                    printf("Serial number must be positive. Try again.\n");
                    continue;
                }

                if (is_serial_used(serials, i, serial)) {
                    printf("Serial number %d already used. Try again.\n", serial);
                    continue;
                }

                valid = 1;
            }

            serials[i] = serial;
            furniture[i].serial_no = serial;
            furniture[i].order = i;
            furniture[i].status = AVAILABLE;
        }

        free(serials);
        printf("Furniture initialization complete.\n");
    }

    return furniture;
}

void furniture_free(furniture_piece *furniture) {
    free(furniture);
}

static const char *status_to_string(piece_status status) {
    switch (status) {
        case AVAILABLE: return "AVAILABLE";
        case MOVING_FORWARD: return "MOVING_FORWARD";
        case MOVING_BACKWARD: return "MOVING_BACKWARD";
        case PLACED:    return "PLACED";
        case BLOCKED:   return "BLOCKED";
        default:        return "UNKNOWN";
    }
}

void furniture_reset_serials(furniture_piece *furniture, int count) {
    int *serials = malloc((size_t)count * sizeof(int));
    if (serials == NULL) {
        perror("malloc");
        return;
    }

    for (int i = 0; i < count; ++i) {
        serials[i] = i + 1;
    }

    shuffle_serials(serials, count);

    for (int i = 0; i < count; ++i) {
        furniture[i].serial_no = serials[i];
        furniture[i].status = AVAILABLE;
    }

    free(serials);
}

void furniture_display_table(furniture_piece *furniture, int count, int round) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║     ROUND %d SHARED FURNITURE ORDER TABLE          ║\n", round);
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Serial No. │   Order   │     Status    │  Index   ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < count; ++i) {
        printf("║    %3d    │    %3d    │   %-11s │   %3d    ║\n",
               furniture[i].serial_no,
               furniture[i].order,
               status_to_string(furniture[i].status),
               i);
    }
    
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Total Pieces: %d\n\n", count);
}

