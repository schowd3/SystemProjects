/* Do Not Modify This File */

#include "clock.h"

/* Local Global Variables */
static int time = 0;

/* Initialize the Clock */
void clock_init(int time_start) {
  time = time_start;
  return;
}

/* Return the Current Clock Time */
int clock_get_time() {
  return time;
}

/* Advance the Clock by 1 */
void clock_advance_time() {
  time++;
  return;
}
