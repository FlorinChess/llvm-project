#include <stdio.h>

#define INPUT_LENGTH 10

int main(void) {
  char input_buffer[10];
  scanf("%s", input_buffer);

  printf("Input: %s\n", input_buffer);

  char null_terminated_buffer[11];
  
  int i = 0;
  for (; i < INPUT_LENGTH; i++) {
    null_terminated_buffer[i] = input_buffer[i];
  }
  null_terminated_buffer[i] = '\0';

  return i;
}