#include <stdlib.h>
#include <stdio.h>

#define BUFFER_SIZE 20

void executeCommand(char* command) {
  system(command);
}


int main() {

  puts("Please input command: \n");
  char buffer[BUFFER_SIZE];

  fgets(buffer, BUFFER_SIZE, stdin);

  printf("%s\n", buffer);

  char new_buffer[BUFFER_SIZE * 2]; 

  for (int i = 0; i < BUFFER_SIZE; i++) {
    new_buffer[i] = '\0';
  }
  printf("%s\n", new_buffer);

  executeCommand(buffer);

  return 0;
}