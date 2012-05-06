/* TCPD.c - TCPD daemon 
 *
 * @author : Dhinesh Dharman
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TCPD.h"
#include "Timer.h"
#include "troll.h"
#include <math.h>

#define USE_TROLL

#define TRUE 1
#define FALSE 0

#define DEBUG 

#define min(a,b) ((a)<(b)?(a):(b))

/* Global variables */
struct sockaddr_in src, dest, replyaddr;
int newConnCount, oldConnCount;
struct hostent* hp;

/* Window variables */
TCP_window sendWindow, recvWindow;
TCP_seq nextSeq = 100;

/* Buffer variables */
TCP_buffer* sBufPtr = NULL;
TCP_buffer* rBufPtr = NULL;
int sBufCount, rBufCount;

int nextExpectedSeq = 100;
int ackCount = 0;
/*Jacobson variables*/
double A = 0, D = 3;
int RTO = 6;
double Err = 0.0, M = 0.0, g = 0.125, h = 0.25;

/*Function prototypes*/
int segmentData (void* buf);
int reassembleData (TCP_segment* ts);
int sendData (void* buf, int buflen, int sock, struct sockaddr_in name);
int recvData (void* buf, int sock, struct sockaddr_in name);
int sendSegment (TCP_segment* ts, int len, int sock, struct sockaddr_in name);
int recvSegment (TCP_segment* ts, int sock, struct sockaddr_in name);
TCP_segment constructSegment (void* buf, int buflen);
void* processSegment (TCP_segment* ts, int len);
int isValidChecksum (TCP_segment* ts, int len);
/*u_short computeChecksum (TCP_segment* ts, int len);*/
int computeRTO ();
int isValidSeqno (TCP_segment* ts);
TCP_buffer* addToBuffer (TCP_buffer*, int* , TCP_segment, int seq);
TCP_buffer* removeFromSendBuffer (TCP_buffer*, int , struct sockaddr_in, int ackno);
void sendFromSendBuffer (TCP_buffer*, int, struct sockaddr_in, struct sockaddr_in, int, TCP_buffer*);
TCP_buffer* sendFromRecvBuffer (TCP_buffer*, int, struct sockaddr_in);
void sendTimerAddRequest (int sock, struct sockaddr_in dest, TCP_segment* tsp, int time);
void sendTimerRemoveRequest (int sock, struct sockaddr_in dest, TCP_segment* tsp);
int updateWindow (TCP_window*, int, int); 
u_short computeChecksum(char *buf, int length, u_short *crc );


void leftshift_3(unsigned char *buffer,int size){
    unsigned char temp;
    int j;
    for(j = 0; j<size; j++)
    {
        buffer[j] = buffer[j]<<1;
        temp = buffer[j+1] & 0x80;
        if(temp)
            buffer[j] = buffer[j] | 0x01;
        else
            buffer[j] = buffer[j] | 0x00;
        if (j == size-1) buffer[j+1] = buffer[j+1]<<1;
    }
}



void leftshift_16(unsigned char *buffer,int size)
{
    unsigned char temp, tem;
    int j;

    for(j = 0; j<size; j++){
        buffer[j] = buffer[j]<<1;
        temp = buffer[j+1] & 0x80;
        
        if(temp)
            buffer[j] = buffer[j] | 0x01;
        else
            buffer[j] = buffer[j] | 0x00;
        
        if(j == (size-1)){
            buffer[j+1] = buffer[j+1] << 1;
            tem = buffer[j+2] & 0x80;
            if(tem)
                buffer[j+1] = buffer[j+1] | 0x01;
            else
                buffer[j+1] = buffer[j+1] | 0x00;
            buffer[j+2] = buffer[j+2] << 1;
        }
    }
}


int main (){

  int sock_main, sock_timer;
  struct sockaddr_in main, timer;
  char buf[TCP_SEGMENT_SIZE];
  char hostname[128];
  gethostname (hostname, sizeof(hostname));
  hp = gethostbyname(hostname); 
  
  sock_main = socket (AF_INET, SOCK_DGRAM, 0);
  
  if (sock_main < 0){
    perror("opening UDP socket for main use");
    exit(1);
  }

  bzero ((char*)&main, sizeof (main));
  main.sin_family = AF_INET;
  main.sin_port = TCPD_PORT;
  bcopy((void *)hp->h_addr_list[0], (void *)&main.sin_addr, hp->h_length);
    
  if (bind(sock_main, (struct sockaddr *)&main, sizeof(main)) < 0){
    perror("getting socket name of main TCPD port");
    exit(2);
  }

  printf("TCPD MAIN SOCKET : PORT = %d ADDR = %d\n",main.sin_port,main.sin_addr.s_addr);  
 
  bzero((char*)&timer, sizeof(timer));
  timer.sin_family = AF_INET;
  timer.sin_port = TIMER_PORT;
  bcopy((void *)hp->h_addr_list[0], (void *)&timer.sin_addr, hp->h_length);
  
  /* TODO : Initialize send and receive windows */
  sendWindow.low = 100;
  sendWindow.high = sendWindow.low + WINDOW_SIZE * TCP_MSS;
  recvWindow.low = 100;
  recvWindow.high = recvWindow.low + WINDOW_SIZE * TCP_MSS;

  while (1){
    
      int type = 0;
      void *ptr = NULL;
      int send_bytes = 0;
      int sock  = 0;
      int rlen = 0;
      int i = 0;
      TCP_segment ts;
      TCP_segment ts_recv;
      TCP_segment* tsp;
      struct sockaddr_in name;
      
      /*
      printf("Inside child process to receive data\n");
      */
      printf("\n\n");
      
      /* Initialize buffer to all zeroes */
      bzero(buf, TCP_MSS);
       
      /* Receive data from UDP port */
      rlen = recvSegment((TCP_segment *)&ts, sock_main, main);
       
      /* Get segment type (LOCAL, REMOTE or TIMER) */
      type = getSegmentType (ts);      

      /*
      printf("Segment Type = %d len = %d\n",type,rlen);
      */
    
      /* Segment received from local application process */
      if (type == LOCAL_SEGMENT) {
        
        printf("Received local segment len = %d\n",rlen);
        printf("sBufCount = %d\n",sBufCount);

        printf("Send window : (%d, %d)\n",sendWindow.low, sendWindow.high);        
        
        /* Receiving destination address when a CONNECT is called*/
        if (ts.hdr.seq == DEST_INFO) {
          memcpy ((void*)&dest, (void*)&(ts.data), sizeof(struct sockaddr_in));
          printf("Received destination info from app\n");
          continue;
        }
        /* Receiving source address when a BIND is called*/
        else if (ts.hdr.seq == SRC_INFO) {
          memcpy ((void*)&src, (void*)&(ts.data), sizeof(struct sockaddr_in));
          printf("Received source info from app port = %d\n",src.sin_port);
          continue;
        }

        /* 
        memcpy((void*)&buf[0], (void*)&ts.data, rlen-TCP_HEADER_SIZE);
        for (i = 0; i < rlen-TCP_HEADER_SIZE; ++i)
          printf("%d", buf[i]);
        printf("Recv bytes = %d\n",rlen-TCP_HEADER_SIZE);
        */
        if (sBufCount == BUFFER_SIZE) continue;
        /*
        sBufCount++;
        */
        /* Construct TCP segment with received data */
        ts = constructSegment((void*)&ts.data, rlen-TCP_HEADER_SIZE);
             
        /* Sequence number gets incremented by 
         * 1 for each byte sent */
        nextSeq = nextSeq + rlen-TCP_HEADER_SIZE;

        /* Add segment to send buffer */
        /* TODO: Revisit buffer overflow logic here */
        
        memcpy((void*)&buf, (void*) &ts.data, rlen);
        /*
        for (i = 0; i < rlen; ++i)
          printf("%c");
        printf("\n");
        */
        sBufPtr = addToBuffer (sBufPtr, &sBufCount, ts, rlen); 
          
        /* Send from send buffer */
        sendFromSendBuffer (sBufPtr, sock_main, dest, timer, SEND_ALL, NULL); 
            
      }
      /* Segment received from remote process */
      
      else if (type == REMOTE_SEGMENT) {
        printf("Received remote segment len = %d\n",rlen);
        
        printf("Recv window : (%d, %d)\n",recvWindow.low, recvWindow.high);        
        
        /* TODO : Checksum here */ 
        /*
        printf("seq = %d crc = %d len = %d\n",ts.hdr.seq,ts.hdr.sum, rlen);
        
        memcpy((void*)&buf[0], (void*)&ts, rlen);
        for (i = 0; i < TCP_HEADER_SIZE; ++i)
          printf("%d",buf[i]);
        printf("\n");
        */

        /* Check if segment has valid checksum */
        if (!isValidChecksum(&ts, rlen)){
          printf("Checksum FAILURE\n");
          continue;
        }
        printf("Checksum SUCCESS\n");

 
        /* Check if ACK flag is set */
        if (isACK(&ts)){
          /* TODO :Handle ACK for already ACKed segments here */
          
            printf("Received ACK no = %d\n", ts.hdr.ack);
          
            /* Remove ACKed segment from buffer */
            sBufPtr = removeFromSendBuffer(sBufPtr, sock_main, timer, ts.hdr.ack);        
          
            /* Maintain ACK Count and compute RTO  */
            ackCount += 1;

            computeRTO ();

            continue;
        }
         
        /* Check if segment has valid checksum */
        /*
        if (!isValidChecksum(&ts, rlen)){
          printf("Checksum FAILURE");
          continue;
        }
        printf("Checksum SUCCESS\n");
        */
       
        /* Process received segment and return ptr to data*/
        ptr = processSegment((TCP_segment *)&ts, rlen);

        setACKFlag(&ts);

        /* TODO : How ACK set?*/
        ts.hdr.ack = ts.hdr.seq + rlen-TCP_HEADER_SIZE;
        if (isWithinWindow(recvWindow, ts.hdr.seq+rlen-TCP_HEADER_SIZE)|| (ts.hdr.seq < recvWindow.low)){
          
          int seq = ts.hdr.seq;            
          printf("Sending ACK for seq = %d\n",ts.hdr.seq);
          ts.hdr.seq = nextSeq;
          ts.hdr.urp = 0;
          ts.hdr.sum = 0;
          memcpy((void*)&buf, (void*)&ts, TCP_HEADER_SIZE);
  
          ts.hdr.sum = computeChecksum(&buf[0], TCP_HEADER_SIZE, &(ts.hdr.sum));
        
          /*
          printf("Checksum for ACK seq : %d crc : %d len : %d\n",ts.hdr.seq, ts.hdr.sum, TCP_HEADER_SIZE);
          
          for (i = 0; i < TCP_HEADER_SIZE; ++i)
            printf("%d",buf[i]);
          printf("\n");
          */
          /*ts.hdr.urp = REMOTE_SEGMENT;*/
          /*
          for (i = 0; i < TCP_HEADER_SIZE; ++i)
            printf("%d",buf[i]);
          printf("\n");
          */
          /* Sending ACK to remote process */
          sendSegment(&ts, TCP_HEADER_SIZE, sock_main, replyaddr);
          
          nextSeq = nextSeq + TCP_HEADER_SIZE;      
          ts.hdr.seq = seq;
        }
         /*printf("Sent ACK for seq = %d to dest = %d\n",ts.hdr.seq, replyaddr.sin_addr.s_addr);
          */
        if (!isWithinWindow(recvWindow, ts.hdr.seq+rlen-TCP_HEADER_SIZE) 
            || nextExpectedSeq > ts.hdr.seq){
            
          printf("Not within recv window seq = %d\n",ts.hdr.seq);
          continue;
        }
          
        printf("rBufCount = %d\n",rBufCount);          
        /*
        if (rBufPtr != NULL)  printf("RB : headseq = %d nes = %d\n",rBufPtr->ts.hdr.seq, nextExpectedSeq);
        if (rBufPtr != NULL && rBufPtr->ts.hdr.seq < nextExpectedSeq){
          printf("EXITING.......... \n");
          exit(1);
        }
        */
        /*
        if (nextExpectedSeq < recvWindow.low){
          printf("EXITING..\n");
          exit(1);
        }
        */
        
        if (rBufCount == BUFFER_SIZE) continue;
        
        /*rBufCount++;
        */
        /* TODO: Temporary hack to avoid recv buffer */
        /*sendData(&ts, rlen, sock_main, src);  
        
        continue;
        */

        /* Add segment to receive buffer */
        rBufPtr = addToBuffer (rBufPtr, &rBufCount, ts, rlen); 
          
        if (rBufPtr == NULL){
          printf("rbufptr = NULL post adding\n");
          continue; 
        }
        /* Update receive window */          
        updateWindow(&recvWindow, ts.hdr.seq, rlen-TCP_HEADER_SIZE); 
          
        /* Send data to local process */
        rBufPtr = sendFromRecvBuffer(rBufPtr, sock_main, src);
      }
      else if (type == TIMER_SEGMENT){

          TCP_buffer* ptr = NULL;

          printf("Received timer segment len = %d\n",rlen);
       
          memcpy((void*)&ptr, (void*)&(ts.data), sizeof(ptr));
          
          /* Update RTO on timeout. Message from timer 
           * implies timeout. */
          computeRTO();
          
          /* Retransmit segment to remote process */    
          sendFromSendBuffer (sBufPtr, sock_main, dest, timer, SEND_ONE, ptr);   
      }
  }  
  return 0;
}

/* Add segment to buffer */
TCP_buffer* addToBuffer (TCP_buffer* bufptr, int* bufcount, TCP_segment ts, int len){
  /* TODO : mapping of seq and buffer ptr. <- Why?*/
  TCP_buffer* ptr = NULL;
  TCP_buffer* prev = NULL;
  TCP_buffer* p = bufptr;
  ptr = (TCP_buffer*) malloc (sizeof(TCP_buffer));
  ptr->ts = ts;
  /*memcpy((void*)&(ptr->ts), (void*)&ts, sizeof(TCP_segment));
  */
  ptr->len = len;
  ptr->txCount = 0;
  ptr->next = NULL;
  
  /*
  if (p != NULL ) printf("\taddToBuffer(): pre IF head seq =%d\n",(p->ts.hdr.seq));
  */
  if (p == NULL) bufptr = ptr;
  else {
    
    /*
    printf("p->ts.hdr.seq = %d ts.hdr.seq = %d\n",p->ts.hdr.seq,ts.hdr.seq);
    */
    while (p != NULL && p->ts.hdr.seq < ptr->ts.hdr.seq){
      prev = p;
      p = p->next;
    }
    /* If p becomes NULL, then ptr is the highest
     * seqno and should be appended */

    if (prev != NULL && prev->ts.hdr.seq == ptr->ts.hdr.seq)
      return bufptr;
    if (p != NULL && p->ts.hdr.seq == ptr->ts.hdr.seq)
      return bufptr;

    if (p == NULL){
      
        ptr->next = prev->next;
        prev->next = ptr;
    
    }
    /* If p does not become NULL, then ptr should
     * be inserted in between prev and p */
    else {
      /* If prev is NULL, then ptr becomes head */
      if (prev  == NULL){
        ptr->next = p;
        bufptr = ptr;
      }
      /* If prev is not NULL, then ptr inserted 
       * between prev and ptr*/
      else{
        prev->next = ptr;
        ptr->next = p;
      }

    }
    /*
    if (p != NULL && p->ts.hdr.seq > ptr->ts.hdr.seq){
      ptr->next = p;
      bufptr = ptr;
    }
    else if (p!= NULL && p->ts.hdr.seq == ptr->ts.hdr.seq){
      
      
      printf("Ignoring duplicate seqno...\n");
      return bufptr;
    }
    else{
      prev->next = ptr;
      ptr->next = p;
    }
    */
  }
  /*
  printf("\taddToBuffer(): post IF added seq = %d head seq = %d\n",ts.hdr.seq,(bufptr)->ts.hdr.seq);
  */
  *bufcount = *bufcount + 1;
  return bufptr;
}

/* Remove ACKed segment from send buffer */
TCP_buffer* removeFromSendBuffer (TCP_buffer* bufptr, int sock, struct sockaddr_in timer, int ackno){
  TCP_buffer* p = bufptr;
  TCP_buffer* prev = NULL; 
  struct timeval tim;
  double t1;
    
  /* TODO: Circular buffer */
  
  /* Search for corresponding segment using ACK no*/
  while (p != NULL && (p->ts.hdr.seq + p->len - TCP_HEADER_SIZE) != ackno){
    prev = p;
    /*
    printf("Traversing seq = %d len = %d\n",p->ts.hdr.seq, p->len);
    */
    p = p->next;
  }
  /* Segment found in buffer. Should always be true!*/
  if (p != NULL){
    if (p == bufptr)  bufptr = p->next;
    else  prev->next = p->next;
    p->next = NULL;
    printf("Removed ACKed segment seq = %d\n",p->ts.hdr.seq);
    /* Update M for RTO */
    
    if (p->txCount == 1){
      gettimeofday(&tim, NULL);
      t1 = tim.tv_sec + (tim.tv_usec/1000000.0);
      M = t1 - p->lastTxTime; 
    }
    /*
     * printf("Updating M.. lastTime = %.2f\n",p->lastTxTime);
    */
    /* Send remove request to timer */
    sendTimerRemoveRequest (sock, timer, &(p->ts));
    /* Update send window */
    
    updateWindow (&sendWindow, p->ts.hdr.seq, p->len - TCP_HEADER_SIZE);
    
    free (p);
    sBufCount--;
    if (sBufCount == 0) return NULL;
    /*
    if (bufptr == NULL) printf("Send buffer empty\n");
    */
  }
  /* Should never happen */
  else {
    printf("Segment not found in buffer for ACK = %d\n",ackno);
  }
  return bufptr;
}

/* Send all unsent segments from buffer to remote destination */
void sendFromSendBuffer (TCP_buffer* bufptr, int sock, struct sockaddr_in dest, struct sockaddr_in timer, int mode, TCP_buffer* ptr){
    /* Traverse the buffer and send valid segments that have not been 
     * sent before. A segment is valid if its seqno is within current 
     * send window. */
    TCP_buffer* p = bufptr;
    struct timeval tim;
    double t1;
    int time = 0;
    if (mode == SEND_ONE){
      p = ptr;
      if (isWithinWindow (sendWindow, p->ts.hdr.seq + p->len) && (p->txCount > 0)){
      
          /*if (fork() == 0){*/
          printf("Sending data...\n");
          /* Sending segment to destination */
          sendSegment (&(p->ts), p->len, sock, dest);

          /* Set timeout value using exponential backoff */
          /*time = (int)(RTO * pow(2.0, (double)p->txCount));
          */
          time = RTO;
          /* Sending request to timer process to add new entry */        
          sendTimerAddRequest (sock, timer, &(p->ts), time);     
          
          printf("Retransmitted segment seq = %d\n",p->ts.hdr.seq);        
          /* Updating number of tries and time of last transmission*/
          p->txCount = p->txCount + 1;
          gettimeofday(&tim, NULL);
          p->lastTxTime = tim.tv_sec + (tim.tv_usec/1000000.0);
          /*
          printf("lastTime = %.2f\n",p->lastTxTime);
          */
      }
      else {
        printf("RT segment not in window seq = %d txcount = %d\n",p->ts.hdr.seq, p->txCount);
        /*
        exit(1);
        */
      }
      
      return;
    }
    while (p != NULL){
      if (isWithinWindow (sendWindow, p->ts.hdr.seq + p->len) && (p->txCount == 0)){
        printf("Sending data...\n");
        /* Sending segment to destination */
        sendSegment (&(p->ts), p->len, sock, dest);
        /* Sending request to timer process to add new entry */
        
        sendTimerAddRequest (sock, timer, &(p->ts), time); 
        
        /*
         * printf("Sent segment seq = %d\n",p->ts.hdr.seq);
        */ 
        /* Updating number of tries */
        p->txCount = p->txCount + 1;
        gettimeofday(&tim, NULL);
        p->lastTxTime = tim.tv_sec + (tim.tv_usec/1000000.0);
        /*
        printf("lastTime = %.2f\n",p->lastTxTime);
        */
        
      }
      else {
        /*
        printf("segment not in window (%d,%d)seq = %d txcount = %d\n",sendWindow.low, sendWindow.high,p->ts.hdr.seq, p->txCount);
        */
      }
      p = p->next;
    }
}
/* Send contiguous segments from recv buffer to application */
TCP_buffer* sendFromRecvBuffer (TCP_buffer* bufptr, int sock, struct sockaddr_in dest){
  TCP_buffer* p = bufptr;
  TCP_buffer* prev = NULL;
  TCP_buffer* temp = NULL;
  int i = 0;
  /* TODO : Circular buffer. So ! NULL should be avoided */
  if (bufptr == NULL) return NULL;
  if (nextExpectedSeq != bufptr->ts.hdr.seq)  return bufptr;

  while (bufptr != NULL){
    /*
    printf("%d,",bufptr->ts.hdr.seq);
    */
    if (nextExpectedSeq == bufptr->ts.hdr.seq){
      /* Update the next expected seqno in order */
      nextExpectedSeq = bufptr->ts.hdr.seq + bufptr->len - TCP_HEADER_SIZE;
      /* Send segment to application */
      sendData(&(bufptr->ts.data), bufptr->len-TCP_HEADER_SIZE, sock, dest);  
      
      /*
      printf("Sent to app seq = %d 1st char = %c\n",bufptr->ts.hdr.seq, bufptr->ts.data[0]);
      printf("rBufCount = %d headseq = %d\n",rBufCount,bufptr->ts.hdr.seq);
      */
      i = 0;
      
      while (i++ < 100000000);
      
      /*
      sleep (1);
      */
      /* Remove sent segment from buffer */
      /*
      if (p == bufptr)  bufptr = p->next;
      else  prev->next = p->next;
      temp = p;
      p = p->next;
      temp->next = NULL;
      */
      temp = bufptr;
      bufptr = bufptr->next;
      temp->next = NULL;
      free (temp);
      rBufCount--;
      p = bufptr;
      /*
      while (p != NULL){
        printf("%d ",p->ts.hdr.seq);
        p = p->next;
      }
      */
      /*
      printf("Post remove rBufCount = %d\n",rBufCount);
      if (bufptr == NULL){
        printf("bufptr = NULL\n");   
        
      } 
      */
      if (rBufCount == 0) return NULL;
      /*    
      printf("headseq = %d\n",bufptr->ts.hdr.seq);
      */
    }
    else break;
    /*
    else {
      prev = p;
      p = p->next;
    }
    */
  }
  /*printf("headseq = %d\n",bufptr->ts.hdr.seq);
  */
  return bufptr;
}

/* Update the window using given seq number*/
int updateWindow (TCP_window* winptr, int seq, int len){
  int i = 0;
  int shift = -1;
  int gap = FALSE;
  int winsize_bytes = WINDOW_SIZE*TCP_MSS;
  /* Check if valid seqno according to window */
  if (!isWithinWindow(*winptr, seq+len)) return -1;
  /* Updating window with latest seq */
   /*
   printf("Setting seq %d of len %d in window (%d, %d)\n",seq, len, winptr->low, winptr->high);
   */
   /*
   for (i = 0; i < len; ++i){
    winptr->window[seq + i - winptr->low] = TRUE;
  }
  */
  memset((void*)&(winptr->window[seq-winptr->low]), TRUE, len);
  /* TODO : Redesign!!! */
  /* Sliding the window after the latest update */
  for (i = 0; i < winsize_bytes; ++i){
    /* Gap in window detected */
    if (winptr->window[i] == FALSE) {
      shift = i;
      break;
    }
    /* If gap in window detected, slide the window to
     * the right by the number of contiguous slots before 
     * gap was detected */
    /*
    if (gap == TRUE){
      
      if (shift == 0) break;
      winptr->window[i-shift] = winptr->window[i];
      
      shift = i;
      break;
    }
    */
    /* If no gap so far, keep track of the number of contiguous
     * filled slots */
    /*
    else if (winptr->window[i] == TRUE){
      shift++;
    }
    */
  }
  if (shift < 0){
     memset((void*)&(winptr->window[0]), FALSE, winsize_bytes);
     winptr->low += winsize_bytes + TCP_MSS;
     winptr->high += winsize_bytes + TCP_MSS;
  }

  else if (shift > 0){
    memmove((void*)&(winptr->window[0]), (void*)&(winptr->window[shift]), winsize_bytes-shift+1);
    memset((void*)&(winptr->window[winsize_bytes-shift+1]), FALSE, shift);
    /*
    for (i = 0; i < winsize_bytes; ++i){
      if (i < (winsize_bytes-shift))
        winptr->window[i] = winptr->window[i+shift];
      else 
        winptr->window[i] = FALSE;
    }
    */
    winptr->low  += shift;
    winptr->high += shift;
  }
  printf("Updated window : (%d,%d)\n",winptr->low, winptr->high);
  return 0;
}

/* Update the recv window using seq number */
int updateRecvWindow (int seq){
    return 0;
}

/* Check if given sequence number is within given window */
int isWithinWindow (TCP_window win, int seq){
  /*return 1;*/
  /*
   printf("Checking window for seq validity\n");
   */
  return (seq >= win.low && seq <= win.high);
}

/* Send a request to timer process to add a new entry */
void sendTimerAddRequest (int sock, struct sockaddr_in dest, TCP_segment* tsp, int time){
  TCP_segment ts;
  int send_bytes = 0;
  timerMsg tmsg;
  bzero((char*)&ts, TCP_SEGMENT_SIZE);
  ts.hdr.urp = TIMER_SEGMENT;

  tmsg.type = ADD_TIMER;
  tmsg.tsp = tsp;
  tmsg.time = time;
  memcpy((void*)&(ts.data), (void*)&tmsg, sizeof(timerMsg));

  send_bytes = sendto (sock, &ts, TCP_HEADER_SIZE + sizeof(timerMsg), 0, (struct sockaddr*)&dest, sizeof(dest));
  if (send_bytes < 0){
    perror ("error sending add request to timer process");
    exit(1);
  }

  /*printf("Sent add request to timer\n");
  */
}
/* Send a request to timer process to remove entry */
void sendTimerRemoveRequest(int sock, struct sockaddr_in dest, TCP_segment* tsp){  
  TCP_segment ts;
  int send_bytes = 0;
  timerMsg tmsg;
  bzero((char*)&ts, TCP_SEGMENT_SIZE);
  ts.hdr.urp = TIMER_SEGMENT;

  tmsg.type = REMOVE_TIMER;
  tmsg.tsp = tsp;
  tmsg.time = 0;
  memcpy((void*)&(ts.data), (void*)&tmsg, sizeof(timerMsg));
  send_bytes = sendto (sock, &ts, TCP_HEADER_SIZE + sizeof(timerMsg), 0, (struct sockaddr*)&dest, sizeof(dest));
  
  if (send_bytes < 0){
    perror ("error sending remove request to timer process");
    exit(1);
  }
  /*
  printf("Sent remove request to timer\n");
  */

}
/* Check if segment is an ACK */
int isACK (TCP_segment* tsp){
  return (tsp->hdr.off_res_flags != 0);
}

/* Set ACK flag for segment */
int setACKFlag (TCP_segment* tsp){
  return (tsp->hdr.off_res_flags = 1);
}

/* Get segment type (LOCAL or REMOTE)*/
int getSegmentType (TCP_segment ts){
  return ts.hdr.urp;
}

/* Set segment type (LOCAL or REMOTE) */
void setSegmentType (TCP_segment* tsp, int type){
  tsp->hdr.urp = type;
}
/* Send TCP segment to remote process */
int sendSegment (TCP_segment* tsp, int len, int sock, struct sockaddr_in name){
  int     troll_len = 0;
  int     send_bytes = 0;
  char    buf[TCP_SEGMENT_SIZE];
  struct  sockaddr_in troll, destaddr;
  in_addr_t IP;
  int   i = 0;
  struct{
    struct sockaddr_in header;
    char body[TCP_SEGMENT_SIZE];
  } msg;

  NetMessage nm;

  if (tsp == NULL) return -1;
  
  /*printf ("sendSegment()\n");
  */
  tsp->hdr.urp = REMOTE_SEGMENT;
  tsp->hdr.sport = 0;
  /*tsp->hdr.dport = name.sin_port;
   * */
  
  name.sin_port = TCPD_PORT;
        
  printf("\tDEST PORT = %d\n\tADDR = %d\n\tseq = %d\n\tlen = %d\n",name.sin_port, name.sin_addr.s_addr, tsp->hdr.seq,len);
  
  memcpy((void*)&buf,(void*)tsp, len);
  
  /*printf("crc = %d\n", tsp->hdr.sum);*/
  /*
  for (i = 0; i < len; ++i){
    printf("%d",buf[i]);
  }
  printf("\n");
  */  
  /*
  bzero((char *)&msg.header, sizeof (msg.header));
  msg.header.sin_family = htons(AF_INET);
  msg.header.sin_addr.s_addr = htons(inet_addr("164.107.112.68"));
  msg.header.sin_port = htons(name.sin_port);
  bcopy((char *)&name.sin_addr, (char *)&msg.header.sin_addr, hp->h_length);
  */
 
  
  bzero((char*)&destaddr, sizeof (destaddr));
  destaddr.sin_family = htons(AF_INET);
  bcopy((char*)&name.sin_addr, (char *)&destaddr.sin_addr, hp->h_length);
  destaddr.sin_port = TCPD_PORT;
  msg.header = destaddr;
  nm.msg_header = destaddr;

  /*
  printf("msg.header.sin_port = %d\n",msg.header.sin_port);
  printf("msg.header.sin_addr.s_addr = %d\n",msg.header.sin_addr.s_addr);
  */
  memcpy ((void*)&(msg.body), (void*)tsp, len);
  memcpy((void*)&nm.msg_contents, (void*)tsp, len);

  bzero((char *)&troll, sizeof (troll));
  troll.sin_family = AF_INET;
  troll.sin_port = htons(TROLL_PORT);
  bcopy(hp->h_addr_list[0], (char *)&troll.sin_addr, hp->h_length);
  /*troll.sin_addr.s_addr = inet_addr("127.0.0.1");*/
  troll_len = sizeof(troll);

#ifndef USE_TROLL
  /* Non-troll version */
  send_bytes = sendto (sock, tsp, len, 0, (struct sockaddr *)&name,sizeof(name));
#else
  /* Troll version */
  /*
  for (i=0;i < len; ++i){
    printf("%d",msg.body[i]);
  }
  printf("\n");
  */
  printf("Sending via troll PORT = %d\n", htons(troll.sin_port));
  send_bytes = sendto(sock, &msg, sizeof(msg.header)+len, 0, (struct sockaddr *)&troll,sizeof(troll));
#endif  
  if(send_bytes < 0){
    perror("sending segment");
    exit(5);
  }
      
  return send_bytes;
}

/* Receive TCP segment from remote process */
int recvSegment (TCP_segment* tsp, int sock, struct sockaddr_in name){
  int name_len = sizeof(name);
  int recv_bytes = 0;
  int sockfd =  0;
  char buf[1036];
  int offset = 0;
  int i = 0;
  recv_bytes = recvfrom(sock, buf, TCP_SEGMENT_SIZE+sizeof(name), 0, (struct sockaddr *)&replyaddr, &name_len);
  if (recv_bytes < 0) {
    perror("receiving from some process");
    exit(5);
  }
  /*
  for (i = 0; i < recv_bytes; ++i)
    printf("%d",buf[offset+i]);
  printf("\n");
  */ 
  if (replyaddr.sin_port == htons(TROLL_PORT)){
    /*
     * printf("Reply port = %d\n",replyaddr.sin_port);
    */
     offset = 16;
    recv_bytes -= 16;
    
    memcpy((void*)&replyaddr, (void*)&buf, 16);
    replyaddr.sin_family = AF_INET;
  }
  memcpy((void*)tsp, (void*)&buf[offset], recv_bytes);
  /*
   * printf("replyaddr.sin_port = %d\n",replyaddr.sin_port);
  */
  /* Remote segment */
  if (replyaddr.sin_port == TCPD_PORT){
    tsp->hdr.urp = REMOTE_SEGMENT;
  }
  /* Timer segment */
  else if (replyaddr.sin_port == TIMER_PORT){
    tsp->hdr.urp = TIMER_SEGMENT;
  }
  else tsp->hdr.urp = LOCAL_SEGMENT;
  
  return recv_bytes;
}

/* 3-bit CRC */

u_short computeChecksum_3(char *buf, int length, u_short *crc )
{
    int i,cnt;
    unsigned char buff[length+1],buff_1[1],Poly[1],res[1],ans[1],crc_ex;

    unsigned short Crc;
            
    unsigned int poly = 0xB0;/*B0 tanslates to 0000 0000 1011 0000 in binary*/
    
    bzero(buff,sizeof(buff));
    memcpy(buff,buf,length); /*input string with an extra byte added to the end */
    
    memcpy(&Crc,crc,2);
    /*printf("CRC copied: %d\n",Crc);*/

    crc_ex = Crc & 0x00FF;
/*    printf("crc_ex = %d\n",crc_ex);*/
    crc_ex = crc_ex << 5;
    memcpy((buff+length),&crc_ex,1);
/*    printf("buff[last_element] = %d\n",buff[length]);*/

/*    printf("Copied character array: %d\n",buff[1]);  */

    memcpy(Poly,&poly,1);
    
    for (i=0; i<length*8; i++)
    {
        memcpy(buff_1,buff,1);
/*        printf("Begin loop memcpy: %d\n",buff_1[0]);*/
        if(buff_1[0] & 0x80)       /*check if first bit is 1*/
        {
            res[0] = Poly[0]^buff_1[0]; /*if 1 then exor with 1011*/
            memcpy(&buff[0],res,1); /*copy back the result to buff*/
        }
/*        printf("Endloop: %d\n",buff_1[0]);*/
        leftshift_3(buff,length);
    }


     ans[0] = buff[0]>>5;

    /*
     * printf("rx crc: %d\n",ans[0]);
    */
     return (u_short)ans[0];
}

/* 16-bit CRC computation */
u_short computeChecksum(char *buf, int length, u_short *crc ){
    
    int i,cnt;
    unsigned char buff[length+2],buff_1[1],Poly[1];
    unsigned short Crc,ans;
    unsigned char poly1 = 0xC0;
    unsigned char poly2 = 0X02;
    unsigned char poly3 = 0x80;
    
    bzero(buff,sizeof(buff));
    memcpy(buff,buf,length); 
    
    memcpy(&Crc,crc,2);
    
    memcpy((buff+length),&Crc,2);
  
    for ( i = 0; i < length*8; i++){
      if(buff[0] & 0x80){
          buff[0] = poly1^buff[0]; 
          buff[1] = poly2^buff[1];
          buff[2] = poly3^buff[2];
      }
      leftshift_16(buff,length);
    }
    memcpy(&ans,buff,2);
    return ans;
    /*printf("crc: %d\n",ans);*/
}



/* Construct TCP segment from data*/
TCP_segment constructSegment (void* ptr, int len){
  
  TCP_segment ts;
  char buf[TCP_SEGMENT_SIZE];
  char testbuf[36] = {0,0,49,117,0,0,0,0,124,0,
                      0,0,0,0,0,0,0,0,0,0,
                      0,0,0,0,0,0,0,0,0,0,
                      2,0, 97, 98, 99, 10};
  int i = 0;
  /* Initialize segment to all zeroes */
  bzero((char*)&ts, TCP_SEGMENT_SIZE);
  /* Populate segment with TCP header fields */
  ts.hdr.sport         = 0;
  ts.hdr.dport         = dest.sin_port;
  ts.hdr.seq           = nextSeq;
  ts.hdr.ack           = 0;
  ts.hdr.off_res_flags = 0;
  ts.hdr.win           = 0;
  ts.hdr.urp           = 0;
  ts.hdr.sum           = 0;

  if (ptr == NULL)  return ts;

  /* Copy data to TCP data field using buffer pointer */
  memcpy((void*)&ts.data, (const void*)ptr, len);
  /* Compute checksum and store in checksum field */
  
  memcpy((void*)&buf, (void*)&ts, TCP_HEADER_SIZE+len);
  /*
   
  for (i = 0; i < (len + TCP_HEADER_SIZE);++i){
    printf("%d ",buf[i]);
  }
  
  printf("\n");
  */
  /*
   * ts.hdr.sum = check_sum (buf, 36);
  */
  /*
  printf("TCP_SEGMENT_SIZE = %d\n",TCP_SEGMENT_SIZE);
  */
  ts.hdr.sum = 0;
  ts.hdr.sum = computeChecksum(&buf[0], TCP_HEADER_SIZE+len, &(ts.hdr.sum));
  /*
  printf("Test checksum = %d\n",ts.hdr.sum);
  */
  return ts;
}

/* Process received TCP segment */
void* processSegment (TCP_segment* tsp, int len){
  void* ptr = NULL;
  int i = 0;
  int crc = 0;
  char buf[TCP_SEGMENT_SIZE];
  if (tsp == NULL) return NULL;
  /*
  u_short temp = tsp->hdr.sum;
  tsp->hdr.sum = 0;
  tsp->hdr.urp = 0;
  */
  memcpy((void*)&buf, (void*)tsp, len);
  /*
  for (i = 0; i < len; ++i){
    printf("%d ", buf[i]);
  }
  printf("\n");
  */
  /*
  if (isValidChecksum(tsp,len)){
    printf("Checksum SUCCESS!\n");
  }
  else {
    printf("Checksum FAILURE\n");
    return NULL;
  }
  */
 /*
  crc = computeChecksum((char*)tsp, len, &temp);
  if (crc != 0){
    printf("\tChecksum failed! Computed crc = %d\n",crc);
    return NULL;
  }
  else printf("\tChecksum succeeded!! Computed crc = %d\n",crc);
  */
  /* Get pointer to data field */
  ptr = (void*) &(tsp->data);
  return ptr;
}

/* Compute checksum of given TCP segment 
u_short computeChecksum (TCP_segment* tsp, int len){

  printf("computeChecksum()\n");

  return 0;
}
*/
/* Check validity of checksum field */
int isValidChecksum (TCP_segment* tsp, int len){
  /*
  int crc = computeChecksum ((char*)tsp, len, &(tsp->hdr.sum)); 
  
  printf("crc is validchecksum = %d\n", crc);
  return 1;
  */
  u_short temp = tsp->hdr.sum;
  tsp->hdr.urp = 0;
  tsp->hdr.sum = 0;
  return (computeChecksum ((char*)tsp, len, &(temp)) == 0);
  
}

/* Check validity of seqno */
int isValidSeqno (TCP_segment* tsp){
  return TRUE;
}

/* Compute RTO (and RTT) using Jacobson's algorithm */
int computeRTO (){
  /* M - latest RTT measurement */
  if (ackCount == 1){
    /* TODO : How to compute A ?*/
    A = M + 1;
    D = A / 2;
  }
  else if (ackCount > 1){
    Err = M - A;
    A = A + g * Err;
    D = D + h * (abs(Err)-D);
  }
  RTO = (int)(A + 4 * D);
  RTO = (RTO == 0)?1: RTO;
  
  RTO = 1;
  printf("Current RTO = %d M = %.2f\n",RTO, M);
  
  
  return 0;
}

/* Send data to application process */
int sendData (void* ptr, int len, int sock, struct sockaddr_in name){
  int name_len = sizeof(name);
  int send_bytes = 0;
  char buf[1000];
  int i = 0;
  if (ptr == NULL)  return -1;
  /*
  printf("ptr = %d len = %d name.sin_addr = %d name.sin_port = %d\n",ptr,len, name.sin_addr.s_addr, name.sin_port);
  */
  send_bytes = sendto(sock, ptr, len, 0, (struct sockaddr *)&name, sizeof(name));
  if (send_bytes < 0){
    perror("sending data to application process");
      exit(5);
  }
  /*
  memcpy((void*)&buf, (void*)ptr, len);
  for (i = 0; i < len; ++i)
    printf("%d",buf[i]);
  */
  /*
  printf("Sent len = %d\n", send_bytes);
  */
  return send_bytes;
}

/* Receive data from application process */
int recvData (void* ptr, int sock, struct sockaddr_in name){
  int name_len = sizeof(name);
  int recv_bytes = 0;
  int i = 0;
  char buf[TCP_MSS];
  if (ptr == NULL) return -1;
  recv_bytes = recvfrom(sock, ptr, TCP_MSS, 0, (struct sockaddr *)&name, &name_len);
  if (recv_bytes < 0) {
    perror("receiving data from application process");
    exit(5);
  }
  return recv_bytes;
}
