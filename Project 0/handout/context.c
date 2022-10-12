/* Main File for CPU Context Switch Program - Project 0 (CS367)
 * Do Not Modify this File 
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "sys.h"

/* Takes one argument: Test Number to Run */
int main(int argc, char *argv[]) {
  /* Check for Command Line Arguments (2) */
  if(argc != 2) {
    printf("Usage: %s <tracefile.dat>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  FILE *fp = fopen(argv[1], "r");
  if(fp == NULL) {
    printf("Error: Could not open %s for reading\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  /* Initialize the Scheduler and Run */
  sys_init(fp);
  sys_run();

  return 0;
}
