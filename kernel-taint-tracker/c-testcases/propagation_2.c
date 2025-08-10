#include <stdio.h>

#define INPUT_LENGTH 10

void print_input(char* input_buffer) {
  printf(input_buffer);        // sink
}

int main(int argc, char* argv[]) {
  char input_buffer[10];       // tainted after scanf
  scanf("%s", input_buffer);   // tainted

  print_input(input_buffer);   // tainted because of param
}