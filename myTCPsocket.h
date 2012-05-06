/*
 * myTCPsocktet.h - Header for custom TCP socket
 *
 * @author : Dhinesh Dharman
 *
 */

#ifndef myTCPsocket_H
#define myTCPsocket_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int SOCKET (int domain, int type, int protocol);
int BIND (int sockfd, struct sockaddr *my_addr, int addrlen);
int ACCEPT (int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int CONNECT (int sockfd, struct sockaddr *serv_addr, int addrlen);
int SEND (int sockfd, const void *msg, int len, int flags);
int RECV (int sockfd, void *buf, int len, int flags); 
int CLOSE (int sockfd);

#endif
