#pragma once

/* Link control for the test harness.
 * Implemented in main.c (which owns GAP), called from uart_proto.c. */

#define HARNESS_OK         0   /* request accepted */
#define HARNESS_BAD_STATE  1   /* not allowed right now (e.g. DISCONNECT with no link) */
#define HARNESS_FAILED     2   /* allowed, but the BLE stack refused */

int harness_start_adv(void);
int harness_stop_adv(void);
int harness_disconnect(void);