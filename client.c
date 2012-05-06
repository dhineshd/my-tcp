/* client.c using TCP */

/* Client for connecting to Internet stream server waiting on port 1040 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include "myTCPsocket.h"
/*#define port "1040"*/   /* socket file name */

#define SLEEP_TIME 1
/* client program called with host name where server is run */
int main(int argc, char *argv[])
{
    int sock;                     /* initial socket descriptor */
    int  rval;                    /* returned value from a read */
    struct sockaddr_in sin_addr; /* structure for socket name
                                 * setup */
    /*char buf[1024] = "Hello in TCP from client";*/     /* message to set to server */

    char remote_port;
    struct hostent *remote_host;
    FILE * fp;
    char* local_filename;
    int  local_filesize;
    char buf[1000];
    int  transfer_size;
    int  bytes_read;
    int  bytes_sent;
    int  count;

    if(argc < 4) {
        printf("usage: ftpc <remote-IP> <remote-port> <local-file-to-transfer>\n");
        exit(1);
    }
    sleep (1);
    /* initialize socket connection in unix domain */
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("error opening stream socket");
        exit(1);
    }

    /* setting up variables based on input arguments */
    local_filename = argv[3];
    /*remote_port = argv[2];*/
    remote_host = gethostbyname(argv[1]);

    /* checking server address validity */
    if(remote_host == 0) {
        fprintf(stderr, "%s: unknown host\n", argv[1]);
        exit(2);
    }

    /* construct name of socket to send to */
    bcopy((void *)remote_host->h_addr_list[0], (void *)&sin_addr.sin_addr, remote_host->h_length);
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_port = htons(atoi(argv[2])); /* fixed by adding htons() */
    
    if (sin_addr.sin_addr.s_addr == 0)  {
      printf("Unable to resolve hostname. Try again\n");
      exit(1);
    }
    
    /* establish connection with server */
    if(CONNECT(sock, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in)) < 0) {
        close(sock);
        perror("error connecting stream socket");
        exit(1);
    }
    printf("Connection established\n");

    /* Opening specified local file */
    if ((fp = fopen(local_filename, "rb")) == NULL){
        fprintf(stderr, "error opening %s\n", local_filename);
        close(sock);
        exit(1);
    }
    /* Getting file size */
    fseek(fp, 0L, SEEK_END);
    /* Converting to network byte order */
    local_filesize = htonl(ftell(fp));
    /* Seeking to the beginning of the file */
    fseek(fp, 0L, SEEK_SET);

    printf("Before calling SEND() sock = %d\n",sock);
    /* send file size to server (4 bytes) */
    if (SEND(sock, (const void *)&local_filesize, 4, 0) < 0){
        perror("error sending file size");
        fclose(fp);
        close(sock);
        exit(1);
    }
    printf("Sent filesize = %d\n",local_filesize);
    sleep (SLEEP_TIME);
    /* send file name to server (20 bytes) */
    if (SEND(sock, (const void *)local_filename, 20, 0) < 0){
        perror("error sending file name");
        fclose(fp);
        close(sock);
        exit(1);
    }
    printf("Sent filename = %s\n",local_filename);
   /* send file data to server in blocks of 1000 bytes */
    /*transfer_size = 1000;*/
    bytes_read = 0;
    bytes_sent = 0;
    count = 0;
    while (!feof(fp)){
        
      sleep(SLEEP_TIME);  
      printf("Sending file data.. \n");
    
      /* read a block from file */
        bytes_read = fread((void *)&buf, 1, 1000, fp);
        if (ferror(fp)){
            perror("error reading from file");
            fclose(fp);
            close(sock);
            exit(1);
        }
        if ((bytes_sent = SEND(sock, (const void *)buf, bytes_read, 0)) < 0){
            perror("error sending file data");
            close(sock);
            fclose(fp);
            exit(1);
        }
        
    }
    /*
    sleep (20);
    */
    printf("FILE SENDING COMPLETED\n");
    fclose(fp);
    close(sock);
    return 0;
}

