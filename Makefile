# Makefile for client and server

CC = gcc
OBJCLI = client.c 
OBJSRV = server.c
OBJTCPD = TCPD.c
OBJTIMER = Timer.c
CFLAGS = -ansi -g -lm
# setup for system
LIBS = 

all: ftpc ftps TCPD Timer

ftpc:	$(OBJCLI) myTCPsocket.o
	$(CC) $(CFLAGS) -o $@ $(OBJCLI) myTCPsocket.o $(LIBS)

ftps:	$(OBJSRV) myTCPsocket.o
	$(CC) $(CFLAGS) -o $@ $(OBJSRV) myTCPsocket.o $(LIBS)

myTCPsocket.o: myTCPsocket.c
	$(CC)	$(CFLAGS) -o $@ myTCPsocket.c -c

TCPD: $(OBJTCPD)
	$(CC) $(CFLAGS) -o $@ $(OBJTCPD)

Timer: $(OBJTIMER)
	$(CC) $(CFLAGS) -o $@ $(OBJTIMER) 

clean:
	rm ftpc ftps TCPD Timer myTCPsocket.o
