#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];
    char data[BUFSIZ] = "Hello World\n";
    char buffer[BUFSIZ];
    
    if (pipe(pipefd) == 0)
    {
        write(pipefd[1], data, strlen(data));
        read(pipefd[0], buffer, BUFSIZ);
        printf("%s", buffer);
    }
    
    return 0;
}
