#ifndef USER_H
#define USER_H
#define SEND_TYPE_ALL 110
#define SEND_TYPE_OTHERS 220

#include <pthread.h>
#include "protocol.h"


#pragma pack(1)
typedef struct User {
    struct User *prev;
    struct User *next;
    pthread_t thread;
    int socketFileDescriptor;
    char name[32];
} User;
#pragma pack(0)

typedef struct mqMessage {
    message message;
    struct User *user;
} mqMessage;

User *addNewUser(pthread_t thread, int socketFileDescriptor, char name[]);

int removeUser(User *user);

int testUserName(const char *nameToTest);

int sendMessageToAllUsers(message *message, int code);

int notifyUserRemoved(User *user, uint8_t code);

User *accessViaSockfd(int sockfd);

int sendSthTo(mqMessage *buffer);

void *unlockMutex();

int notifyUserAdded(User *user);

int getSockfd(const char *username);

#endif
