#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#define WR_FILE_IDX     1
#define WR_STRING_IDX   2

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    if(argc != 3) {
        syslog(LOG_ERR, "Usage: %s <writefile> <writestr>\n", argv[0]);
        return 1;
    }
    syslog(LOG_DEBUG, "Writing %s to %s\n", argv[WR_STRING_IDX], argv[WR_FILE_IDX]);
    FILE * pFile = fopen(argv[WR_FILE_IDX], "wt");
    if(!pFile) {
        int local_errno = errno;
        perror("Failed to open\n");
        syslog(LOG_ERR, "Failed to open %s. Got errno %d\n", argv[WR_FILE_IDX], local_errno);
        return 1;
    }
    fwrite(argv[WR_STRING_IDX], 1, strlen(argv[WR_STRING_IDX]), pFile);
    fclose(pFile);
    return 0;
}