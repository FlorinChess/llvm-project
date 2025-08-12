#include <stdio.h>
#include <string.h>

#define USERNAME_SIZE 40
#define PASSWORD_SIZE 20

char username[USERNAME_SIZE];
char password[PASSWORD_SIZE];

int main(void) {
  printf("Input your username: ");  
  scanf("%s", username);

  printf("Input your password: ");
  scanf("%s", password);
  
  if (strcmp(password, "admin1234")) {
    printf("Welcome back %s!\n", username);
  }
  return 0;
}
