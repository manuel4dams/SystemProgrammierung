#ifndef BROADCASTAGENT_H
#define BROADCASTAGENT_H
#define QUEUE_NAME "/message_queue"


#include "user.h"
#include "protocol.h"

struct User;

int broadcastAgentStart(void);

int broadcastAgentPut(mqMessage *msg);

void *pauseServer(void);

void *resumeServer(void);

int isMqFull(void);

#endif
