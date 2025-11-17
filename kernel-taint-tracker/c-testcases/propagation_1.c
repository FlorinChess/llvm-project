#include <stdio.h>
#include <stdlib.h>

#define INPUT_LENGTH 10
#define ADDRESS 0x000844000

int main(void) {
  char input_buffer[INPUT_LENGTH];           // tainted after scanf
  scanf("%s", input_buffer);       // tainted

  realloc((void*)ADDRESS, 10);

  printf(input_buffer);            // sink
}