#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void){
  FILE *writeFP;
  char buffer[BUFSIZ + 1];

  sprintf(buffer, "Hello World"); //print hello world to buffer

  writeFP = popen("od -c", "w"); // od show content of a file in octal format, use -c to show in human readable format
  if(writeFP){
    fwrite(buffer, sizeof(char), strlen(buffer), writeFP);  // write to buffer using writeFP stream
    pclose(writeFP); // close the pipe
    exit(EXIT_SUCCESS);
  }
  exit(EXIT_FAILURE);
}