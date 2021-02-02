#include <pthread.h>
#include <mqueue.h>
#include "protocol.h"
#include "broadcastagent.h"
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include "user.h"
#include "util.h"

static mqd_t messageQueue;
static pthread_t threadId;
static sem_t queueLock;

static int full;

void *pauseServer(void) {
    sem_wait(&queueLock);
    return NULL;
}

void *resumeServer(void) {
    sem_post(&queueLock);
    return NULL;
}

static void *broadcastAgent(void *arg) {
    sem_init(&queueLock, 0, 1);
    mqMessage *tmpMessage = malloc(sizeof(mqMessage));


    while (1) {
        if (mq_receive(messageQueue, (char *) tmpMessage, sizeof(mqMessage), 0) == -1) {
            errnoPrint("error receiving message from message queue");
            break;
        }
        sem_wait(&queueLock);
        sendSthTo(tmpMessage);
        sem_post(&queueLock);
    }
    debugPrint("exciting bcastagent");
    free(tmpMessage);
    return arg;
}

int broadcastAgentStart(void) {
    struct mq_attr mq_attr;

    mq_attr.mq_maxmsg = 10;
    mq_attr.mq_msgsize = sizeof(mqMessage);
    mq_attr.mq_flags = 0;

    if ((messageQueue = mq_open(QUEUE_NAME, O_RDWR | O_CREAT, 0660, &mq_attr)) == -1) {
        errnoPrint("error creating message queue");
        return -1;
    }
    if (mq_unlink(QUEUE_NAME) == -1) {
        errnoPrint("error unlinking message queue");
        return -1;
    }
    if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
        errnoPrint("error creating broadcast agent's thread");
        return -1;
    }
    return 1;
}


int broadcastAgentPut(mqMessage *msg) {
    struct timespec *abs_timeout = malloc(sizeof(struct timespec));
    abs_timeout->tv_nsec = 25;
    abs_timeout->tv_sec = 0;
    if (mq_timedsend(messageQueue, (char *) msg, sizeof(mqMessage), 0, abs_timeout) == -1) {
        full = 1;
    } else {
        full = 0;
    }
    return 1;
}

int isMqFull(void) {
    debugPrint("returning full with %d", full);
    return full;
}
