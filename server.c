/* server.c using TCP */

/* Server for accepting an Internet stream connection on port 1040 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include "myTCPsocket.h"
/*#define port "1040"*/   /* socket file name */

/* server program called with local port as argument */
int main(int argc, char *argv[])
{
    int sock;                     /* initial socket descriptor */
    int msgsock;                  /* accepted socket descriptor,
                                 * each client connection has a
                                 * unique socket descriptor*/
    int rval=1;                   /* returned value from a read */
    struct sockaddr_in sin_addr; /* structure for socket name setup */
    char buf[1000]; /* buffer for holding read data */
    /*char buf2[1024] = "Hello back in TCP from server";*/
    int bytes_recvd = 0;
    int local_port;
    struct hostent* hp;
    char hostname[128];
    int  i = 0;
    if(argc < 2) {
        printf("usage: ftps <local-port>\n");
        exit(1);
    }
    sleep (1);
    printf("TCP server waiting for remote connection from clients ...\n");
    
    /*initialize socket connection in unix domain*/
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("error opening stream socket");
        exit(1);
    }

    /* construct name of socket to send to */
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_addr.s_addr = INADDR_ANY;
    sin_addr.sin_port = htons(atoi(argv[1]));

    gethostname (hostname, sizeof(hostname));
    hp = gethostbyname(hostname);
    bcopy((void*)hp->h_addr_list[0],(void*)&sin_addr.sin_addr, hp->h_length);
    
    /* bind socket name to socket */
    if(BIND(sock, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in)) < 0) {
        perror("error binding stream socket");
        exit(1);
    }
    

    /* listen for socket connection and set max opened socket connetions to 5 */
    /*listen(sock, 5);*/

    /*while (1){*/
        /* accept a (1) connection in socket msgsocket */
        /*               
        if((msgsock = ACCEPT(sock, (struct sockaddr *)NULL, (int *)NULL)) == -1) {
            perror("error connecting stream socket");
            exit(1);
        }
        */
        /* creating child process to handle request */
        /*if (fork() == 0){*/
            int filesize;
            char filename[31] = "server_dir/";
            int ret;
            FILE *fp;
            /* put all zeros in buffer (clear) */
            bzero(buf,1000);

            /* receiving filesize (4 bytes) from client */
            ret = RECV(sock, (void *)&filesize, sizeof(filesize), 0);
            if(ret < 0) {
                perror("error reading filesize on stream socket");
                close(msgsock);
                exit(1);
            }

            /* client closed connection during/after data transfer */
            if (ret == 0){
                perror("connection closed by client");
                close(msgsock);
                exit(1);
            }

            /* Network to host byte order conversion */
            filesize = ntohl(filesize);
            printf("Received file size = %d\n", filesize);

            /* receiving filename (20 bytes) from client */
            ret = RECV(sock, (void *)&filename[11], 20, 0);
            if(ret < 0) {
                perror("error reading filename on stream socket");
                close(msgsock);
                exit(1);
            }
            /* client closed connection during/after data transfer */
            if (ret == 0){
                perror("connection closed by client");
                close(msgsock);
                exit(1);
            }

            printf("Received file name = %s\n", &filename[11]);
            /* Creating file to write incoming data */
            if ((fp = fopen(filename, "wb")) == NULL){
                fprintf(stderr, "error creating %s\n", filename);
                close(msgsock);
                exit(1);
            }
            printf("Opened %s in write mode\n", filename);

          
            while (1){
                /* read from msgsock and place in buf */
                int bytes_written = 0;
                ret = RECV(sock, (void *)buf, sizeof(buf), 0);
                if(ret < 0) {
                    perror("error reading on stream socket");
                    fclose(fp);
                    close(msgsock);
                    exit(1);
                }
                /* client closed connection during/after data transfer */
                /*
                if (ret == 0) {
                    perror("connection closed by client");
                    fclose(fp);
                    close(msgsock);
                    exit(1);
                }
                */
                /*
                printf("Received file data..ret = %d\n",ret);
                */
                /*
                for (i = 0; i < ret; ++i)
                  printf("%c",buf[i]);
                printf("\n");
                */
                /* write received bytes to specified file */
                if ((bytes_written = fwrite((const void *)buf, 1, ret, fp)) != ret){
                        perror("error writing to file");
                        fclose(fp);
                        close(msgsock);
                        exit(1);
                }
                bytes_recvd += bytes_written;
                /*
                printf("Bytes recvd : %d\n",bytes_recvd);
                */
                if(bytes_recvd == filesize) break;  

            }
        /*}*/
       /* close(msgsock);*/
    /*}*/
    close(sock);
    sleep (2);
    printf("\n\nFILE TRANSFER COMPLETED.\n");
    return 0;
}
