#include "user.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
struct User *firstUser = NULL;
struct User *lastUser = NULL;

void *unlockMutex() {
    pthread_mutex_unlock(&userLock);
    return NULL;
}

User *GetNewUser(pthread_t thread, int socketFileDescriptor, char name[]) {
    struct User *newUser = (struct User *) malloc(sizeof(struct User));
    if (newUser == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(newUser, 0, sizeof(User));
    newUser->thread = thread;
    newUser->socketFileDescriptor = socketFileDescriptor;
    strncpy(newUser->name, name, sizeof(newUser->name));
    newUser->prev = NULL;
    newUser->next = NULL;
    return newUser;
}

User *addNewUser(pthread_t thread, int socketFileDescriptor, char name[]) {
    pthread_mutex_lock(&userLock);

    User *newUser = GetNewUser(thread, socketFileDescriptor, name);
    if (newUser == NULL) {
        return NULL;
    } else if (firstUser == NULL) {
        firstUser = newUser;
        lastUser = newUser;
    } else if (lastUser != NULL) {
        lastUser->next = newUser;
        newUser->prev = lastUser;
        lastUser = newUser;
    }
    return newUser;
}

int removeUser(User *userToRemove) {
    pthread_mutex_lock(&userLock);
    int status = 1;
    // only one user
    if (userToRemove == lastUser &&
        userToRemove == firstUser) {
        lastUser = NULL;
        firstUser = NULL;
        status = 2;
    }
        // User is first user in list
    else if (userToRemove == firstUser) {
        User *userTmp = userToRemove->next;
        userToRemove->next->prev = NULL;
        firstUser = userTmp;
    }
        // User is last user in list
    else if (userToRemove == lastUser) {
        User *userTmp = userToRemove->prev;
        userToRemove->prev->next = NULL;
        lastUser = userTmp;;
    }
        // User is somewhere in the middle of the list
    else if (userToRemove->next != NULL || userToRemove->prev != NULL) {
        userToRemove->prev->next = userToRemove->next;
        userToRemove->next->prev = userToRemove->prev;
    } else {
        pthread_mutex_unlock(&userLock);
        status = -1;
    }
    close(userToRemove->socketFileDescriptor);
    free(userToRemove);
    pthread_mutex_unlock(&userLock);
    return status;
}


int notifyUserAdded(User *user) {
    message *tmp = malloc(sizeof(message));
    User *currentUser = firstUser;
    if (firstUser == NULL) {
        // if user list is empty return -1
        return -1;
    }
    while (currentUser != NULL && strcmp(currentUser->name, "") != 0) {


        if (sendUserAdded(tmp, currentUser->socketFileDescriptor, user->name, SEND_USER_ADDED_TYPE_NOTIFY) == -1) {
            debugPrint("sending user added message to %s", user->name);
            return -1;

        }

        if (currentUser->socketFileDescriptor != user->socketFileDescriptor) {
            debugPrint("sending user added message to %s", currentUser->name);
            sendUserAdded(tmp, user->socketFileDescriptor, currentUser->name, SEND_USER_ADDED_TYPE_UPDATE);
        }

        currentUser = currentUser->next;
    }

    free(tmp);
    return 1;
}

int notifyUserRemoved(User *user, uint8_t code) {
    pthread_mutex_lock(&userLock);
    message *tmpMessage = malloc(sizeof(message));
    User *currentUser = firstUser;
    memset(tmpMessage, 0, sizeof(message));
    if (firstUser == NULL) {
        // if user list is empty return -1
        return -1;
    } else {
        while (currentUser != NULL/* && strcmp(currentUser->name, "") != 0*/) {
            memset(tmpMessage->messageBody.userRemoved.name, 0, sizeof(tmpMessage->messageBody.userRemoved.name));

                if (currentUser != user) {
                    //debugPrint("sending user removed to %s", currentUser->name);
                    sendUserRemoved(tmpMessage, currentUser->socketFileDescriptor, user->name, code);
                }

            currentUser = currentUser->next;

        }
    }
    pthread_mutex_unlock(&userLock);
    free(tmpMessage);
    return 1;
}

int sendMessageToAllUsers(message *message, int code) {
    User *currentUser = firstUser;
    if (firstUser == NULL) {
        // if user list is empty return -1
        return -1;
    } else {
        while ((currentUser != NULL)) {
            sendServerMessage(message, currentUser->socketFileDescriptor, "", code, "");
            currentUser = currentUser->next;
        }
        return 1;
    }
}

int sendSthTo(mqMessage *buffer) {
    if (buffer == NULL) {
        return -1;
    }
    int sendType = buffer->message.messageHeader.type;
    if (sendType == USER_REMOVED) {
        sendType = SEND_TYPE_OTHERS;
    } else {
        sendType = SEND_TYPE_ALL;
    }

    User *currentUser = firstUser;
    if (currentUser == NULL) {
        return -1;
    }
    while (currentUser != NULL && strcmp(currentUser->name, "") != 0) {
        switch (sendType) {
            case SEND_TYPE_ALL:
                if (sendSth(&buffer->message, currentUser->socketFileDescriptor) == -1) {
                    errnoPrint("error sending message in sendSthTo");
                    return -1;
                }
                break;
            case SEND_TYPE_OTHERS:
                debugPrint("SEND_TYPE_OTHERS to: %s", currentUser->name);
                if (currentUser->socketFileDescriptor != buffer->user->socketFileDescriptor &&
                    strcmp(currentUser->name, "") != 0) {
                    if ((sendSth(&buffer->message, currentUser->socketFileDescriptor) == -1)) {
                        errnoPrint("error sending message in sendSthTo");
                        return -1;
                    }
                }
                break;
        }
        currentUser = currentUser->next;
    }
    return 1;
}

int getSockfd(const char *username) {
    User *currentUser = firstUser;
    debugPrint("looking for user %s", username);
    while (currentUser != NULL) {
        debugPrint("checking %s", currentUser->name);
        if (strcmp(username, currentUser->name) == 0) {
            debugPrint("found User %s", username);

            return currentUser->socketFileDescriptor;
        } else {
            currentUser = currentUser->next;
        }
    }
    return -1;
}

int testUserName(const char *nameToTest) {
    User *currentUser = firstUser;

    while (currentUser != NULL) {
        if (strcmp(currentUser->name, nameToTest) == 0) {
            return -1;
        } else { currentUser = currentUser->next; }
    }
    return 1;
}

void printUsers() {
    User *curr = malloc(sizeof(User));
    curr = firstUser;
    while (curr != NULL) {
        infoPrint("USER: %s", curr->name);
        curr = curr->next;
    }
    infoPrint("\n");
}

User *accessViaSockfd(int sockfd) {
    User *curr = malloc(sizeof(User));
    curr = firstUser;
    while (curr != NULL) {
        if (curr->socketFileDescriptor == sockfd) {
            break;
        }
        curr = curr->next;
    }
    return curr;
}
