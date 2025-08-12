#include <stdio.h>
#include <stdlib.h>

#define INPUT_LENGTH 10

int main(void) {
  char* input_buffer = malloc(INPUT_LENGTH);           // tainted after scanf
  scanf("%s", input_buffer);       // tainted

  // memeory is freed but that does not guarantee
  // that it is also erased
  free(input_buffer);              

  printf(input_buffer);            // sink
}