/* TCPD.h - TCPD header file
 *
 *@author Dhinesh Dharman 
 *
 */

#ifndef TCPD_H
#define TCPD_H

#define u_short unsigned short int
#define u_long  unsigned long int
#define TCP_seq unsigned int

#define TCP_HEADER_SIZE (sizeof(TCP_header))
#define TCP_MSS 1000
#define TCP_SEGMENT_SIZE (sizeof(TCP_segment))

#define TCPD_PORT 29000

#define LOCAL_SEGMENT 1
#define REMOTE_SEGMENT 2
#define TIMER_SEGMENT 3
#define DEST_INFO 1
#define SRC_INFO 2

#define TROLL_PORT 15000

#define ACK_MASK 0xffff

/* Send buffer flags */
#define SEND_ONE 1
#define SEND_ALL 2

#define BUFFER_SIZE 64
#define WINDOW_SIZE 20
/* TCP Header structure */
typedef struct{
  u_short sport;
  u_short dport;
  TCP_seq seq;
  TCP_seq ack;
  u_short off_res_flags;
  u_short win;
  u_short sum;
  u_short urp;

}TCP_header;

/* TCP Segment structure */
typedef struct{
  TCP_header  hdr;
  char        data[TCP_MSS];
}TCP_segment;

/* TCP Buffer Node */
struct _TCPBUF_ {
  TCP_segment ts;
  int len;
  int txCount;
  double lastTxTime;
  struct _TCPBUF_* next;
};
typedef struct _TCPBUF_ TCP_buffer;

/* TCP Window structure */
typedef struct _TCPWIN_ {
  int   low;
  int   high;
  char  window[WINDOW_SIZE*TCP_MSS];
} TCP_window;

#endif
