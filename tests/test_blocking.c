#include <stdio.h>
#include <stdlib.h>
#include "constants.h"
#include "furniture.h"

int main(void) {
    int count = 3;
    furniture_piece *f = malloc((size_t)count * sizeof(furniture_piece));
    if (!f) return 1;

    /* Arrange pieces so that orders are: index0 order=1 serial=4, index1 order=2 serial=3, index2 order=0 serial=1 */
    f[0].serial_no = 4; f[0].order = 1; f[0].status = AVAILABLE;
    f[1].serial_no = 3; f[1].order = 2; f[1].status = AVAILABLE;
    f[2].serial_no = 1; f[2].order = 0; f[2].status = AVAILABLE;

    int expected_order = 0;

    printf("Initial inventory:\n");
    furniture_display_table(f, count, 1);

    /* 1) select serial 4 */
    int idx = 0;
    if (f[idx].status == AVAILABLE) {
        printf("source selected serial %d with status AVAILABLE\n", f[idx].serial_no);
        f[idx].status = MOVING_FORWARD;
        printf("0 -> 1: serial=%d (order=%d, status=MOVING_FORWARD)\n", f[idx].serial_no, f[idx].order);
    }

    /* sink receives and checks */
    if (f[idx].order == expected_order) {
        f[idx].status = PLACED;
        printf("sink: serial %d CORRECT -> PLACED\n", f[idx].serial_no);
        expected_order++;
    } else {
        f[idx].status = MOVING_BACKWARD;
        printf("sink: serial %d WRONG expected %d -> returning\n", f[idx].serial_no, expected_order);
        /* returned to source */
        f[idx].status = BLOCKED;
        printf("RETURN: serial %d marked BLOCKED. No blocked pieces released.\n", f[idx].serial_no);
        furniture_display_table(f, count, 1);
    }

    /* 2) select serial 3 */
    idx = 1;
    if (f[idx].status == AVAILABLE) {
        printf("source selected serial %d with status AVAILABLE\n", f[idx].serial_no);
        f[idx].status = MOVING_FORWARD;
        printf("0 -> 1: serial=%d (order=%d, status=MOVING_FORWARD)\n", f[idx].serial_no, f[idx].order);
    }

    if (f[idx].order == expected_order) {
        f[idx].status = PLACED;
        printf("sink: serial %d CORRECT -> PLACED\n", f[idx].serial_no);
        expected_order++;
    } else {
        f[idx].status = MOVING_BACKWARD;
        printf("sink: serial %d WRONG expected %d -> returning\n", f[idx].serial_no, expected_order);
        /* returned to source */
        f[idx].status = BLOCKED;
        printf("RETURN: serial %d marked BLOCKED. No blocked pieces released.\n", f[idx].serial_no);
        furniture_display_table(f, count, 1);
    }

    /* Verify that the previously blocked serial 4 remains BLOCKED */
    printf("After second return, inventory should show both 4 and 3 as BLOCKED:\n");
    furniture_display_table(f, count, 1);

    /* 3) select serial 1 which is order 0 -> success */
    idx = 2;
    if (f[idx].status == AVAILABLE) {
        printf("source selected serial %d with status AVAILABLE\n", f[idx].serial_no);
        f[idx].status = MOVING_FORWARD;
        printf("0 -> 1: serial=%d (order=%d, status=MOVING_FORWARD)\n", f[idx].serial_no, f[idx].order);
    }

    if (f[idx].order == expected_order) {
        f[idx].status = PLACED;
        printf("SUCCESS: serial %d placed. Releasing blocked pieces.\n", f[idx].serial_no);
        expected_order++;
        for (int j = 0; j < count; ++j) {
            if (f[j].status == BLOCKED) {
                f[j].status = AVAILABLE;
                printf("Releasing blocked serial: %d\n", f[j].serial_no);
            }
        }
        furniture_display_table(f, count, 1);
    } else {
        printf("unexpected: test sequence failed\n");
    }

    free(f);
    return 0;
}
