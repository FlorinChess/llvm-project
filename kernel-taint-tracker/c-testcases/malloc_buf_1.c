#include <stdio.h>
#include <stdlib.h>

#define INPUT_LENGTH 10

int main(void) {
  char* input_buffer = malloc(INPUT_LENGTH);           // tainted after scanf
  scanf("%s", input_buffer);       // tainted

  printf(input_buffer);            // sink
}