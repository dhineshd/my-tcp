#ifndef TIMER_H
#define TIMER_H

#include "TCPD.h"

#define TIMER_PORT 25001
#define CLOCK_PORT 25002
#define ADD_TIMER 1
#define REMOVE_TIMER 2

typedef struct _timerMsg_ {
  int type;
  TCP_segment* tsp;
  int time;
} timerMsg;

void addToDeltaList (TCP_segment* tsp, int time);
void removeFromDeltaList (TCP_segment* tsp);
void updateDeltaList (int num);

#endif
