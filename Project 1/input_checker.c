#include <stdio.h>
#include <stdlib.h>

int main() {
  float val = 0;
  char line[255] = {0};
  printf(".----------------------------------------------------\n");
  printf("| Input Checker for Project 1 (Floating Point)\n");
  printf("+----------------------------------------------------\n");
  printf("| When you type in a value to MLKY, it will be\n");
  printf("|  converted to a float before it is passed in\n");
  printf("|  to your compute_fp function.  Since floats \n");
  printf("|  also round, the number passed in to compute_fp\n");
  printf("|  may be different than your input!\n|\n");
  printf("| This program shows you the actual values passed\n");
  printf("|  into compute_fp when you give an input to MLKY.\n");
  printf("| Enter a value to check, or 0 to quit.\n");

  do{
    printf("+----------------------------------------------------\n");
    fprintf(stderr, "| Enter a value to check: ");
    fgets(line, 255, stdin);
    val = (float)strtod(line, NULL);
    if(val != 0) {
      printf("| You entered:       %s", line);
      printf("| Calling compute_fp(%.40lf)\n", val);
    }
  } while(val != 0);
}
