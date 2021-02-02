#include "clientthread.h"
#include "user.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "protocol.h"
#include "broadcastagent.h"


void *clientthread(void *arg) {
    char userName[31];
    char msg[512];
    int code;
    int checkStatus = 1;


    debugPrint("Client thread[%zi] started.", (ssize_t) pthread_self());

    User *thisUser = (User *) arg;
    mqMessage *testMessage = malloc(sizeof(mqMessage));
    message *newMessage = malloc(sizeof(message));

    if (newMessage == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(newMessage, 0, sizeof(message));

    receiveHeader(&newMessage->messageHeader, thisUser->socketFileDescriptor);
    if (newMessage->messageHeader.type == LOGIN_REQUEST) {
        if ((code = receiveLoginRequest(newMessage,
                                        thisUser->socketFileDescriptor)) == -1 ||
            code == LOGIN_RESPONSE_STATUS_NAME_TAKEN || code == LOGIN_RESPONSE_STATUS_NAME_INVALID ||
            code == LOGIN_RESPONSE_STATUS_PROTOCOL_VERSION_MISMATCH ||
            code == LOGIN_RESPONSE_STATUS_OTHER_SERVER_ERROR) {
        } else {
            debugPrint("name = %s", newMessage->messageBody.loginRequest.name);
            memset(userName, 0, sizeof(userName));
            memset(thisUser->name,0,sizeof(thisUser->name));
            strncpy(userName, newMessage->messageBody.loginRequest.name,
                    strlen(newMessage->messageBody.loginRequest.name));
            strncpy(thisUser->name, userName, strlen(userName));
            testMessage->user = thisUser;
            thisUser = addNewUser(thisUser->thread, thisUser->socketFileDescriptor, thisUser->name);
        }
        if (sendLoginResponse(newMessage, thisUser->socketFileDescriptor, (uint8_t) code) == -1 ||
            code == LOGIN_RESPONSE_STATUS_NAME_TAKEN || code == LOGIN_RESPONSE_STATUS_NAME_INVALID ||
            code == LOGIN_RESPONSE_STATUS_PROTOCOL_VERSION_MISMATCH ||
            code == LOGIN_RESPONSE_STATUS_OTHER_SERVER_ERROR) {
        } else {
            debugPrint("sent login response to %s", thisUser->name);
            if (notifyUserAdded(thisUser) == -1) {
                checkStatus = -1;
            }
            unlockMutex();
            while (checkStatus == 1) {
                memset(testMessage->message.messageBody.server2Client.text, 0,
                       sizeof(testMessage->message.messageBody.server2Client.text));
                if (receiveHeader(&newMessage->messageHeader, thisUser->socketFileDescriptor) == 0) {
                    debugPrint("header = 0, closing..");
                    if (notifyUserRemoved(thisUser, USER_REMOVED_STATUS_CONN_CLOSED_BY_CLIENT) == -1) {
                        errnoPrint("failed to notifyUserRemoved");
                        return NULL;
                    }
                    break;
                }
                // switch just in case there are more cases to be handled
                switch (newMessage->messageHeader.type) {
                    case CLIENT_2_SERVER:
                        if (receiveClientMessage(newMessage,
                                                 thisUser->socketFileDescriptor) == 0) {

                        } else {
                            debugPrint("REDIRECTING MESSAGE TO %d", thisUser->socketFileDescriptor);
                            memset(msg, 0, sizeof(msg));
                            strncpy(msg, newMessage->messageBody.client2Server.text,
                                    strlen(newMessage->messageBody.client2Server.text));
                            if (isMqFull() == 1) {
                                if (sendServerMessage(newMessage, thisUser->socketFileDescriptor, "",
                                                      SERVER_CODE_GENERAL_PROBLEMS, "") == -1) {
                                    errnoPrint("error sending server message");
                                }
                            } else {
                                if (prepareServerMessage(&testMessage->message, thisUser->name,
                                                         SERVER_CODE_CLIENT_MESSAGE,
                                                         msg) == NULL) {
                                    errnoPrint("error preparing server message");
                                    break;
                                }
                                broadcastAgentPut(testMessage);
                            }
                        }
                        break;
                    default:
                        break;
                }

            }
        }
    }
    debugPrint("Client thread[%zi] stopping.", (ssize_t) pthread_self());
    infoPrint("User %s disconnected!", thisUser->name);
    removeUser(thisUser);
    free(newMessage);
    return NULL;
}
