#include <stdio.h>

#define INPUT_LENGTH 10

void print_input(char* input_buffer) {
  printf(input_buffer);        // sink
}

void pass_input(char* input_buffer) {
  print_input(input_buffer);   // tainted because of param

  // tainted because it point to the 
  // same tainted memory object
  char* intermediate_buffer = input_buffer;

  print_input(intermediate_buffer); // also tainted
}

int main(int argc, char* argv[]) {
  char input_buffer[INPUT_LENGTH];       // tainted after scanf
  scanf("%s", input_buffer);   // tainted

  pass_input(input_buffer);    // tainted because of param
}