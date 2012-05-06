/**
 * myTCPsocket.c - Custom TCP sockets module
 *
 * @author : Dhinesh Dharman
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TCPD.h"

struct sockaddr_in dest;
struct sockaddr_in src;
struct hostent* hp;

/* Open a custom TCP socket */
int SOCKET (int domain, int type, int protocol){
  return socket (AF_INET, SOCK_DGRAM, 0);
}

/* Bind custom TCP socket to specified address */
int BIND (int sockfd, struct sockaddr *my_addr, int addrlen){
  char buf[1017]; /* MSS = 1000 Remote address = 16 */
  struct sockaddr_in tcpd;
  struct hostent *hp, *gethostbyname();
  int TCPD_len = sizeof(struct sockaddr_in);
  int send_bytes = 0;
  int ret = 0;
  char hostname[128];
  int msg_offset = 1;
  int sock = 0;  
  TCP_segment ts;
  src.sin_family = my_addr->sa_family;
  memcpy((void*)&(src.sin_port), (const void*)&(my_addr->sa_data), addrlen);
  
  src.sin_port = ntohs(src.sin_port);

  printf("BIND() : SRC PORT = %d ADDR = %lu\n",src.sin_port, src.sin_addr.s_addr);  
  
  bzero((void*)&ts, TCP_SEGMENT_SIZE);
  memcpy((void*)&ts.data, (void*)&src, sizeof (src));
  ts.hdr.seq = SRC_INFO;
  ts.hdr.urp = LOCAL_SEGMENT;
  tcpd.sin_family = AF_INET;
  tcpd.sin_port = TCPD_PORT;
  tcpd.sin_addr.s_addr = src.sin_addr.s_addr;
  
  sock = socket (AF_INET, SOCK_DGRAM, 0);
  ret = sendto (sock, &ts, TCP_HEADER_SIZE + sizeof (struct sockaddr_in), 0, (struct sockaddr *)&tcpd, sizeof(tcpd));
  if (ret < 0){
    perror ("error sending to TCPD process");
    exit(1);
  }
  printf("BIND () : Sent src.. bytes = %d\n",ret);
  close(sock);

  return bind(sockfd, my_addr, addrlen);
}

/* Accept connection on specified socket */ 
int ACCEPT (int sockfd, struct sockaddr *addr, socklen_t *addrlen){
  /* Receive 1 byte from TCPD process to denote data recv from client */
  return socket (AF_INET, SOCK_DGRAM, 0);
}

/* Connect to specified address */
int CONNECT (int sockfd, struct sockaddr *serv_addr, int addrlen){
  int ret = 0;
  int sock = 0;
  char hostname[128];
  TCP_segment ts;
  struct sockaddr_in tcpd;
  struct hostent* hp;
  /* Store destination address */
  dest.sin_family = serv_addr->sa_family;
  memcpy((void*)&(dest.sin_port), (const void*)&(serv_addr->sa_data), addrlen);
  dest.sin_port = ntohs(dest.sin_port);
  bzero((void*)&ts, TCP_SEGMENT_SIZE);
  memcpy((void*)&ts.data, (void*)&dest, sizeof (dest));
  ts.hdr.seq = DEST_INFO;
  ts.hdr.urp = LOCAL_SEGMENT;
  printf("LS = %d\n",ts.hdr.urp);
  tcpd.sin_family = AF_INET;
  tcpd.sin_port = TCPD_PORT;
    
  gethostname (hostname, sizeof(hostname));
  hp = gethostbyname(hostname);
  bcopy((void *)hp->h_addr_list[0], (void *)&tcpd.sin_addr, hp->h_length);
   
  sock = socket (AF_INET, SOCK_DGRAM, 0);
  ret = sendto (sock, &ts, TCP_HEADER_SIZE + sizeof (struct sockaddr_in), 0, (struct sockaddr *)&tcpd, sizeof(tcpd));
  if (ret < 0){
    perror ("error sending to TCPD process");
    exit(1);
  }
  close(sock);

  return 0;
}

/* Send message on specified socket */
int SEND (int sockfd, const void * msg, int len, int flags){
  
  char buf[TCP_SEGMENT_SIZE]; /* MSS = 1000 Remote address = 16 */
  struct sockaddr_in tcpd;
  struct hostent *hp, *gethostbyname();
  int TCPD_len = sizeof(struct sockaddr_in);
  int send_bytes = 0;
  int ret = 0;
  int i = 0;
  char hostname[128];
  int msg_offset = 1;
  TCP_segment ts;
  
  tcpd.sin_family = AF_INET;
  tcpd.sin_port = TCPD_PORT;
  tcpd.sin_addr.s_addr = INADDR_ANY;  
    
  gethostname (hostname, sizeof(hostname));
  hp = gethostbyname(hostname);
  bcopy((void *)hp->h_addr_list[0], (void *)&tcpd.sin_addr, hp->h_length);
  
  /*
  printf("TCPSocket Sending PORT = %d ADDR = %lu\n",tcpd.sin_port, tcpd.sin_addr.s_addr);  
  */
  sockfd = socket (AF_INET, SOCK_DGRAM, 0);

  while (len > 0){
    
    int copy_len = (len > (TCP_MSS))? (TCP_MSS):len;
    /* Initialize buffer to all zeroes */
    bzero((void*)&ts, TCP_SEGMENT_SIZE);
    ts.hdr.urp = LOCAL_SEGMENT;
    ts.hdr.seq = 0;
    memcpy((void *)&(ts.data), (void *)msg, copy_len);
    /* Send data via TCPD process */
    ret = sendto (sockfd, &ts, copy_len + TCP_HEADER_SIZE, flags, (struct sockaddr *)&tcpd, sizeof(tcpd));
    if (ret < 0){
      perror ("error sending to TCPD process");
      exit(1);
    }
    /*
    for (i = 0; i < copy_len; ++i)
      printf("%d",ts.data[i]);
    
    printf("\n");
    */
    len = len - copy_len;
    
  }
  close(sockfd);
  
  return send_bytes;
}

/* Receive message on specified socket */
int RECV (int sockfd, void *buf, int len, int flags){
  
  struct sockaddr_in TCPD;
  struct sockaddr_in tcpd;
  int TCPD_len; 
  int ret;
  char b[1050];
  sockfd = socket (AF_INET, SOCK_DGRAM, 0);
  
  tcpd.sin_family = AF_INET;
  tcpd.sin_port = src.sin_port;
  tcpd.sin_addr.s_addr = src.sin_addr.s_addr;  
  
  bind (sockfd, (struct sockaddr *)&tcpd, sizeof(struct sockaddr_in));
  /* Receive data via TCPD process */
  ret = recvfrom (sockfd, buf, TCP_MSS, flags, (struct sockaddr*)&TCPD, &TCPD_len);
  if (ret < 0){
    perror("error receiving data via TCPD process");
    exit(1);
  }
  /*
  printf("RECV() : ret = %d\n",ret); 
  */
  close(sockfd);
  return ret;
}

int CLOSE (int sockfd){
  return close(sockfd);
}
