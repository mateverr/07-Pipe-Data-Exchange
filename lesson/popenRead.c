#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  FILE *read_fp;
  char buffer[BUFSIZ + 1];
  int chars_read;

  memset(buffer, '\0', sizeof(buffer));
  read_fp = popen("uname -a", "r");  // right paramenter specify that this end is for read
  // uname -a -> OS name with details such as version, release date, hardware name and processor type.
  if (read_fp != NULL) {
    chars_read = fread(buffer, sizeof(char), BUFSIZ, read_fp);
    if (chars_read > 0) {
      printf("\nOutput from pipe: \n%s\n", buffer);
    }
    pclose(read_fp); // close the pipe
    exit(EXIT_SUCCESS);
  }
  exit(EXIT_FAILURE);
}