
#include "message_slot.h"

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


int main(int argc, char *argv[]){
    int file_descriptor;
    int res;
    if (argc != 3){
        printf("wrong number of command line arguments passed  \n");
        exit(-1);
    }

    char * file_path = argv[1];
    unsigned int channel_id = atoi(argv[2]);

    file_descriptor = open( file_path, O_RDWR );
    if( file_descriptor < 0 ) {
        printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
        exit(-1);
    }

    if (ioctl( file_descriptor, MSG_SLOT_CHANNEL, channel_id)<0){
        fprintf(stderr, "Error in ioctl %s\n", strerror(errno));
        exit(1);
    }
    char buffer[BUF_LEN];
    res = read( file_descriptor, buffer, BUF_LEN);
    if(res < 0) {
        fprintf(stderr, "Error in read %s\n", strerror(errno));
        exit(1);
        }

    if (close(file_descriptor)<0){
        fprintf(stderr, "Error in close %s\n", strerror(errno));
        exit(1);
    }
    //fprintf("%s",buffer);
    if (write(1, buffer, res) != res) {
        fprintf(stderr, "Error in write %s\n", strerror(errno));
        exit(1);
    }
    exit(0);
}
