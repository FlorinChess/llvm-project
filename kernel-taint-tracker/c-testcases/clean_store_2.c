#include <stdio.h>
#include <stdlib.h>

#define INPUT_LENGTH 10

void clean_buffer(char* input_buffer) {
  input_buffer = NULL;       // this gets cleaned, so printf should not be reached by tainted data
  printf(input_buffer);      // sink
}

int main(void) {
  char* input_buffer = malloc(INPUT_LENGTH);           // tainted after scanf
  scanf("%s", input_buffer);       // tainted

  clean_buffer(input_buffer);
}