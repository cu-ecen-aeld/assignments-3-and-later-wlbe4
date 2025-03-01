#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <pthread.h>

#define TMP_FILE_NAME   "/dev/aesdchar"

int main(int argc, char const *argv[])
{
    int filefd = open(TMP_FILE_NAME, O_CREAT | O_RDWR | O_APPEND, 0644);
    char buffer[16];
    int offset = 0;
    int numbytes;
    for (int i = 1; i<argc; i++)
    {
        if (write(filefd, argv[i], strlen(argv[i])) == -1)
        {
            perror("write");
            return 1;
        }

        if (write(filefd, "\n", 1) == -1)
        {
            perror("write");
            return 1;
        }
    }
    while(1)
    {
        numbytes = pread(filefd, buffer, sizeof(buffer), offset);
        // printf("Read %d bytes from offset %d\n", numbytes, offset);
        if (numbytes == -1)
        {
            perror("read");
            return 1;
        }
        else if (numbytes == 0)
        {
            break;
        }
        else
        {
            buffer[numbytes] = '\0';
            printf("%s",buffer);
            offset += numbytes;
        }
    }
    close(filefd);
    return 0;
}