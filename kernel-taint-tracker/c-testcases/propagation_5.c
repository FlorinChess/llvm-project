#include <stdio.h>

#define INPUT_LENGTH 10

void print_input(char* input_buffer, int count) {
  for (int i = 0; i < count; i++)
    printf(input_buffer);        // sink
}

int main(int argc, char* argv[]) {
  char input_buffer[INPUT_LENGTH];       // tainted after scanf
  scanf("%s", input_buffer);   // tainted

  print_input(input_buffer, 5);   // tainted because of param
}