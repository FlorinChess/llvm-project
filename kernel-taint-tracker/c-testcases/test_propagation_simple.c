#include <stdio.h>

#define INPUT_LENGTH 10

int main(void) {
  char input_buffer[10];           // tainted after scanf
  scanf("%s", input_buffer);       // tainted

  printf(input_buffer);            // sink
}