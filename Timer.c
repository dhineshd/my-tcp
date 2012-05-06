#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "TCPD.h"
#include "Timer.h"

struct _DL_ {
  
  TCP_segment* tsp;
  int init;
  int curr;
  int startTime;
  struct _DL_* next;
};
typedef struct _DL_ DeltaList; 

/*Delta list structure */
DeltaList* head = NULL;

struct hostent* hp;

int sock_timer, sock_clock;
    
int main(){
  fd_set read_fd_set;
  struct timeval tv;
  struct sockaddr_in name, timer, clock;
  int name_len = 0;
  char hostname[128];
  TCP_segment ts;
  timerMsg tmsg;
  int ret = 0;
  /* Initiliaze the socket set */
  tv.tv_usec = 0;
  tv.tv_sec = 10;
  int timeDiff = 0;
  struct timeval tim;
  double oldTime, currTime;
  gettimeofday(&tim, NULL);
  currTime = tim.tv_sec + (tim.tv_usec/1000000.0);
  

  gethostname (hostname, sizeof(hostname));
  hp = gethostbyname (hostname);
    
  sock_timer = socket (AF_INET, SOCK_DGRAM, 0);
  
  if (sock_timer < 0){
    perror("opening timer socket");
    exit(1);
  }
  
  bzero((char*)&timer, sizeof(timer));
  timer.sin_family = AF_INET;
  timer.sin_port = TIMER_PORT;
  bcopy((void *)hp->h_addr_list[0], (void *)&timer.sin_addr, hp->h_length);
   
  if (bind(sock_timer, (struct sockaddr *)&timer, sizeof(timer)) < 0){
    perror("getting socket name of timer port");
    exit(2);
  }
  printf("TIMER SOCKET : PORT = %d ADDR = %d\n",timer.sin_port, timer.sin_addr.s_addr);

  sock_clock = socket (AF_INET, SOCK_DGRAM, 0);  
  
  if (sock_clock < 0){
    perror("opening clock socket");
    exit(1);
  }  
  
  bzero((char*)&clock, sizeof(clock));
  clock.sin_family = AF_INET;
  clock.sin_port = CLOCK_PORT;
  bcopy((void *)hp->h_addr_list[0], (void *)&clock.sin_addr, hp->h_length);
   
  if (bind(sock_clock, (struct sockaddr *)&clock, sizeof(clock)) < 0){
    perror("getting socket name of clock port");
    exit(2);
  }
 
  printf("CLOCK SOCKET : PORT = %d ADDR = %d\n",clock.sin_port, clock.sin_addr.s_addr);
 
  
  if (fork() == 0){
    int sock = socket (AF_INET, SOCK_DGRAM, 0);
    int a = 0;
    while (1){
      if (sendto (sock, &a, 4, 0, (struct sockaddr*)&clock, sizeof(clock)) < 0){;
        perror("error sending tick from clock to timer");
        exit(1);
      }
      /*
      printf("Clock sending tick...\n");
      */
      sleep(1);
    }
    /*printf("!!!!!!!!!! Clock process exiting !!!!!!!!!!!!!!!!!!\n");
    */
    exit(0);
  }

  
 while (1){
   
    /*
     * printf("Alive....................\n");
    */
    FD_ZERO(&read_fd_set);
    FD_SET(sock_timer, &read_fd_set);
    FD_SET(sock_clock, &read_fd_set);
    
  
    /* Sleep for a fixed duration */
    /*sleep (1);*/
      
    /* Update delta list to indicate elapsed time */
    /*updateDeltaList(1);*/
      
    if (select(sock_clock+1, &read_fd_set, NULL, NULL, &tv) < 0){
      perror ("error on timer select socket");
      /*exit(1);
       * */
    }
  
    if (FD_ISSET(sock_timer, &read_fd_set)){
      int ret = recvfrom(sock_timer, &ts, TCP_SEGMENT_SIZE, 0, (struct sockaddr *)&name, &name_len);  
      if (ret < 0){
        perror("error in timer recv after select");
        /*exit(1);*/
      }
      
      memcpy((void*)&tmsg, (void*)&(ts.data), ret - TCP_HEADER_SIZE);
      /*
      printf("Timer received message type = %d\n",tmsg.type);
      */ 
      if (tmsg.type == ADD_TIMER){
          
        addToDeltaList (tmsg.tsp, tmsg.time);
      }
      else if (tmsg.type == REMOVE_TIMER){
          
        removeFromDeltaList (tmsg.tsp);
      }
      else {
        printf("Unknown tmsg type %d\n", tmsg.type);
        exit(1);
      }
    }
    
    if (FD_ISSET(sock_clock, &read_fd_set)){
      /* Update delta list to indicate elapsed time */
      int ret = recvfrom(sock_clock, &ts, TCP_SEGMENT_SIZE, 0, (struct sockaddr *)&name, &name_len);  
      if (ret < 0){
        perror("error in clock recv after select");
        /*exit(1);*/
      }
      /*
      gettimeofday(&tim, NULL);
      currTime = tim.tv_sec + (tim.tv_usec/1000000.0);
  
      timeDiff = currTime - oldTime;
      oldTime = currTime;
      if (timeDiff > 0.9 && timeDiff < 1.1){
      */
        /*printf("--Clock tick--\n");
         */
        updateDeltaList(1);
      /*}*/
    }
  }
  return 0;
}
/* Add a new entry to the delta list */
void addToDeltaList (TCP_segment* tsp, int t){
  DeltaList* ptr = (DeltaList*) malloc(sizeof(DeltaList));
  struct timeval tim;
  double t1;
  int c = 0;
  
  printf("addToDeltaList()\n");
  
  gettimeofday(&tim, NULL);
  t1 = tim.tv_sec + (tim.tv_usec/1000000.0);
  
  ptr->tsp = tsp;
  ptr->init = ptr->curr = t;
  ptr->startTime = t1; 
  ptr->next = NULL;

  /*printf("addToDeltaList()\n");
  */
  /* Insert node as the head if DL is empty*/
  if (head == NULL){
    head = ptr;
  }
  /* Insert node at start, middle or the end */
  else{
    DeltaList* p = head;
    DeltaList* prev = NULL;
    
    while (p != NULL && (p->curr + p->startTime) < (ptr->init + ptr->startTime)){
      prev = p;
      p = p->next;
    }
    
    ptr->next = p;
    if (prev != NULL){
      prev->next = ptr;
    }
    else head = ptr;
  }
   
  printf("New entry added to timer DL\n");
  
}

/* Remove entry from the delta list */
void removeFromDeltaList (TCP_segment* tsp){
  DeltaList* p = head;
  DeltaList* prev = NULL;
  /*
  printf("removeFromDeltaList()\n");
  */
  while (p != NULL && tsp != p->tsp){
    prev = p;
    p = p->next;
  }
  /* Node found */
  if (tsp == p->tsp){
    /* If head to be deleted, setting new head */
    if (p == head){
      head = p->next;  
    }
    else{
      prev->next = p->next;
    }
    p->next = NULL;
    
   /*
   printf("Entry removed from timer DL\n");
   */
    free(p);
    return;
  }
  /* Node not found */
  else {
    printf("Specified tsp = %d not in delta list\n",tsp);
  }
}

/* Send retransmission request to TCPD */
void sendRTRequest (DeltaList* p){  
  struct sockaddr_in name;
  int sock = 0;
  int send_bytes = 0;
  int i = 0;
  TCP_segment ts;
  printf("Triggering retransmission\n");
  bzero (&ts, TCP_SEGMENT_SIZE);
  bzero (&name, sizeof(struct sockaddr_in));
  ts.hdr.urp = TIMER_SEGMENT;
  /*ts.data = p->tsp;*/
  memcpy((void*)&(ts.data), (void*)&(p->tsp), sizeof(p->tsp));

  name.sin_family = AF_INET;
  name.sin_port = TCPD_PORT;
  bcopy((void *)hp->h_addr_list[0], (void *)&name.sin_addr, hp->h_length);
  
  /*
   * printf("Timer sending RT len = %d\n",TCP_HEADER_SIZE+sizeof(p->tsp));
  */
  /*
   * sock = socket (AF_INET, SOCK_DGRAM, 0);    
  */
  i = 0;
  /*  
  while (i++ < 10000000);
  */
  send_bytes = sendto (sock_timer, &ts, TCP_HEADER_SIZE + sizeof(p->tsp), 0, (struct sockaddr*)&name, sizeof(name));
    
  if (send_bytes < 0){
    perror("error sending RT request from timer to TCPD");
    exit(1);
  }
  /*
   close(sock);
  */
}

/* Update delta list by <interval> secs to reflect time elapsed */
void updateDeltaList (int interval){
  DeltaList* p = head;
  DeltaList* ptr = NULL;
  TCP_segment ts;
  int sock = 0;
  int send_bytes = 0;
  struct sockaddr_in name;
  struct timeval tim;
  double t1;
  int count = 0;
  static double oldTime = 0;
  gettimeofday(&tim, NULL);
  t1 = tim.tv_sec + (tim.tv_usec/1000000.0);
  /*printf("Time = %d\n",(int)t1);*/
  /*
  if (oldTime > 0) interval = (int)t1 - (int) oldTime;
  else interval = 1;
  */
  if (head == NULL) return;
  
  /* Update head entry and send RT request to TCPD
   * if timed out. Then check if other nodes have 
   * timed out and set new head as un-timed out
   * segment.*/

  printf("Updating delta list\n");
  head->curr = head->curr - interval;
  if (head->curr > 0) {
    ptr = head;
    
    while (ptr != NULL){
      /*printf("%d-",ptr->curr);
      */
      count++;
      ptr = ptr->next;
    }
    /*
    printf("DL count = %d head->curr= %d\n",count, head->curr);
    */
    return;
  }
  do{
    int  i = 0;
    while (i++ < 1000000);
    sendRTRequest(head);
    p = head;
    head = head->next;
    p->next = NULL;
    free (p);
    if (head == NULL) break;
    head->curr = head->curr - (p->init - (head->startTime - p->startTime));
  }while(head->curr == 0);
   
  oldTime = t1;
}

