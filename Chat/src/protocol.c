#include "protocol.h"
#include <string.h>
#include "util.h"
#include "user.h"
#include <stdlib.h>
#include "broadcastagent.h"
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>

const char *notificationInvalidCommand = "Invalid command.";
const char *notificationInvalidPermissions = "Invalid permissions for that command.";
const char *notificationGeneralProblem = "Problem with forwarding messages, message queue full.";
const char *notificationServerPaused = "Server halted.";
const char *notificationDontKickYourself = "Please do not kick yourself, disconnect instead";
const char *notificationServerResumed = "Server resumed.";
const char *notificationAlreadyPaused = "Cannot halt server, already halted";
const char *notificationCannotResume = "Cannot resume server, not paused";
const char *commandKick = "/kick";
const char *commandPause = "/pause";
const char *commandResume = "/resume";
ssize_t headerSize = sizeof(uint8_t) + sizeof(uint16_t);
bool isPaused = false;

void *processCommand(const char *command, size_t len, int sockfd) {
    message *tmpMessage = malloc(sizeof(message));
    mqMessage *tmpMqMessage = malloc(sizeof(mqMessage));
    User *thisUser = accessViaSockfd(sockfd);
    if (strncmp(thisUser->name, "Admin", sizeof(accessViaSockfd(sockfd)->name)) != 0) {
        sendServerMessage(tmpMessage, sockfd, "", SERVER_CODE_INVALID_PERMISSIONS, "");
        return NULL;
    } else {
        char buf[len + 1];
        strncpy(buf, command, len);
        buf[len + 1] = '\0';
        if (strncmp(command, commandKick, strlen(commandKick)) == 0) {
            strtok(buf, " ");
            debugPrint("kick command entered");
            tmpMqMessage->user = thisUser;
            char *userToBeKicked = strtok(NULL, " ");
            if (userToBeKicked == NULL) {
                return NULL;
            }
            int sockfdToBeKicked = getSockfd(userToBeKicked);
            if (sockfdToBeKicked == -1) {
                debugPrint("couldnt find username %s", userToBeKicked);
                return NULL;
            }
            User *toBeKicked = accessViaSockfd(sockfdToBeKicked);
            if (strcmp(toBeKicked->name, "Admin") == 0) {
                sendServerMessage(tmpMessage, sockfd, "", SERVER_CODE_DO_NOT_KICK_YOURSELF, "");
                return NULL;
            }
            if (notifyUserRemoved(toBeKicked, USER_REMOVED_STATUS_KICKED_FROM_SERVER) == -1) {
                errnoPrint("error sending notifyUserRemoved");
            }
            pthread_cancel(toBeKicked->thread);
            pthread_join(toBeKicked->thread, NULL);
            removeUser(toBeKicked);
            //close(sockfdToBeKicked);
        } else if (strncmp(command, commandPause, strlen(commandPause)) == 0) {
            debugPrint("sending SERVER_CODE_PAUSED");
            if (!isPaused) {
                isPaused = true;
                memset(tmpMqMessage->message.messageBody.server2Client.text, 0,
                       sizeof(tmpMqMessage->message.messageBody.server2Client.text));
                prepareServerMessage(&tmpMqMessage->message, "", SERVER_CODE_PAUSED, "");
                sendMessageToAllUsers(&tmpMqMessage->message, SERVER_CODE_PAUSED);
                pauseServer();
            } else {
                sendServerMessage(tmpMessage, sockfd, "", SERVER_CODE_ALREADY_PAUSED, "");
            }
        } else if (strncmp(command, commandResume, strlen(commandResume)) == 0) {
            debugPrint("sending SERVER_CODE_RESUMED");
            if (isPaused) {
                memset(tmpMqMessage->message.messageBody.server2Client.text, 0,
                       sizeof(tmpMqMessage->message.messageBody.server2Client.text));
                prepareServerMessage(&tmpMqMessage->message, "", SERVER_CODE_RESUMED, "");
                sendMessageToAllUsers(&tmpMqMessage->message, SERVER_CODE_RESUMED);
                resumeServer();
                isPaused = false;
            } else {
                sendServerMessage(tmpMessage, sockfd, "", SERVER_CODE_CANNOT_RESUME, "");
            }
        } else {
            sendServerMessage(tmpMessage, sockfd, "", SERVER_CODE_INVALID_COMMAND, "");
        }
        infoPrint("%s entered", buf);
    }
    free(tmpMessage);
    free(tmpMqMessage);
    return NULL;
}

int validateType(int type) {
    if (type >= 0 && type <= 5) {
        return 1;
    } else return -1;
}

int validateLength__(int min, int max, uint16_t val) {
    if (val < min || val > max) {
        return -1;
    }
    return 1;
}


ssize_t receiveHeader(messageHeader *buffer, int sockfd) {
    ssize_t bytesRead;
    if ((bytesRead = recv(sockfd, buffer, sizeof(buffer->type) + sizeof(buffer->length), 0)) < 0) {
        if (errno == ECONNRESET) {
            return -1;
        }
        return -1;
    }
    if (bytesRead == 0) {
        return 0;
    }
    if ((ssize_t) bytesRead < (ssize_t) (sizeof(buffer->length) + sizeof(buffer->type))) {
        errnoPrint("read too few bytes from messageHeader");
        return -1;
    }

    buffer->length = ntohs(buffer->length);
    debugHexdump(buffer, sizeof(buffer->length) + sizeof(buffer->type), "messageHeader");
    if (validateType(buffer->type) == -1) {
        errnoPrint("invalid type");
        close(sockfd);
        return -1;
    }
    return (int) (sizeof(buffer->length) + sizeof(buffer->type));
}

int receiveLoginRequest(message *buffer, int sockfd) {
    ssize_t bytesRead;
    uint32_t magicExpected = MAGIC_LOGIN_REQUEST;
    char *bufJump = (char *) buffer;


    if ((bytesRead = recv(sockfd, bufJump + headerSize, buffer->messageHeader.length, MSG_WAITALL)) < 0) {
        errnoPrint("error receiving loginRequest body");
        errnoPrint("bytesRead %zi, sizeof %zi", bytesRead, sizeof(&buffer));
        return -1;
    }

    if (validateLength__(USERNAME_MIN, USERNAME_MAX, (uint16_t) strlen(buffer->messageBody.loginRequest.name)) == -1) {
        return LOGIN_RESPONSE_STATUS_NAME_INVALID;
    }

    if (validateLength__(LENGTH_MIN, LENGTH_MAX, buffer->messageHeader.length) == -1) {
        errnoPrint("invalid length");
        return -1;
    }

    if (bytesRead == 0) {
        return 0;
    }
    buffer->messageBody.loginRequest.version = ntohs(buffer->messageBody.loginRequest.version);
    debugHexdump(bufJump + headerSize, buffer->messageHeader.length, "loginRequest");
    buffer->messageBody.loginRequest.magic = ntohl(buffer->messageBody.loginRequest.magic);
    if (bytesRead < buffer->messageHeader.length) {
        errnoPrint("too few bytes from loginRequest read");
        return -1;
    }
    if (memcmp(&buffer->messageBody.loginRequest.magic, &magicExpected, sizeof(magicExpected)) == 0) {
    } else {
        errnoPrint("corrupted message");
        close(sockfd);
        return -1;
    }
    if (buffer->messageBody.loginRequest.version != VERSION) {
        return LOGIN_RESPONSE_STATUS_PROTOCOL_VERSION_MISMATCH;
    }
    if (nameBytesValidate(buffer->messageBody.loginRequest.name, sizeof(buffer->messageBody.loginRequest.name)) !=
        buffer->messageHeader.length - sizeof(buffer->messageBody.loginRequest.magic) -
        sizeof(buffer->messageBody.loginRequest.version)) {
        errorPrint("Name invalid!");
        return LOGIN_RESPONSE_STATUS_NAME_INVALID;
    }
    if (sizeof(buffer->messageBody.loginRequest.name) < USERNAME_MIN ||
        sizeof(buffer->messageBody.loginRequest.name) > USERNAME_MAX) {
        errnoPrint("corrupted name");
        close(sockfd);
        return -1;
    }
    if (testUserName(buffer->messageBody.loginRequest.name) == -1) {
        return LOGIN_RESPONSE_STATUS_NAME_TAKEN;
    }
    return LOGIN_RESPONSE_STATUS_SUCCESS;
}

int sendHeader(messageHeader *buffer, int sockfd) {
    ssize_t bytesSend;
    if ((bytesSend = send(sockfd, buffer, sizeof(buffer), 0)) < 0) {
        errnoPrint("error sending message header");
        return -1;
    }
    if (bytesSend < (ssize_t) sizeof(buffer)) {
        errnoPrint("sent too few bytes of message header");
        return -1;
    }
    return 1;
}

int sendLoginResponse(message *buffer, int sockfd, uint8_t code) {
    ssize_t bytesSend;
    buffer->messageBody.loginResponse.magic = htonl(MAGIC_LOGIN_RESPONSE);
    buffer->messageBody.loginResponse.code = code;
    strcpy(buffer->messageBody.loginResponse.serverName, SERVER_NAME);
    buffer->messageHeader.length =
            sizeof(buffer->messageBody.loginResponse.magic) + sizeof(buffer->messageBody.loginResponse.code) +
            strlen(
                    buffer->messageBody.loginResponse.serverName);
    buffer->messageHeader.length = htons(buffer->messageHeader.length);
    buffer->messageHeader.type = LOGIN_RESPONSE;

    if (nameBytesValidate(buffer->messageBody.loginResponse.serverName,
                          sizeof(buffer->messageBody.loginResponse.serverName)) !=
        (ssize_t) ntohs(buffer->messageHeader.length) - sizeof(buffer->messageBody.loginResponse.magic) -
        sizeof(buffer->messageBody.loginResponse.code)) {
        errnoPrint("corrupted server name");
        close(sockfd);
        return -1;

    }
    signal(SIGPIPE, SIG_IGN);
    if (fcntl(sockfd, F_GETFD) == -1) {
        return -1;
    }
    if ((bytesSend = send(sockfd, buffer,
                          ntohs(buffer->messageHeader.length) + sizeof(buffer->messageHeader.length) +
                          sizeof(buffer->messageHeader.type),
                          0)) < 0) {
        if (errno == EPIPE) {
            return -1;
        }
        errnoPrint("error sending login response");
        return -1;
    }
    if (bytesSend < (ssize_t) (ntohs(buffer->messageHeader.length) + sizeof(buffer->messageHeader.length) +
                               sizeof(buffer->messageHeader.type))) {
        errnoPrint("sent too few bytes of login response");
        return -1;
    }
    debugHexdump(buffer, ntohs(buffer->messageHeader.length) + sizeof(buffer->messageHeader.length) +
                         sizeof(buffer->messageHeader.type),
                 "sending login response");
    return 1;
}

int receiveClientMessage(message *buffer, int sockfd) {
    ssize_t bytesRead;
    char *bufJump = (char *) buffer;
    if (buffer->messageHeader.length > TEXT_MAX) {
        errnoPrint("invalid text length");
        return -1;
    }
    memset(buffer->messageBody.client2Server.text, 0, sizeof(buffer->messageBody.client2Server.text));
    if ((bytesRead = recv(sockfd, bufJump + headerSize, buffer->messageHeader.length, 0)) < 0) {
        errnoPrint("error receiving client message");
        return -1;
    }
    if (bytesRead == 0) {
        return 0;
    }
    if (bytesRead < buffer->messageHeader.length) {
        errnoPrint("too few bytes of client Message read");
        return -1;
    }
    debugHexdump(bufJump + headerSize, buffer->messageHeader.length, "client message");
    if (buffer->messageBody.client2Server.text[0] == '/') {
        processCommand(buffer->messageBody.client2Server.text, strlen(buffer->messageBody.client2Server.text) + 1,
                       sockfd);
        return 0;
    } else {

    }
    return 1;
}

int sendUserAdded(message *buffer, int sockfd, char *username, uint8_t type) {
    ssize_t bytesSend;
    memset(buffer->messageBody.userAdded.name, 0, sizeof(buffer->messageBody.userAdded.name));
    buffer->messageHeader.type = USER_ADDED;
    if (type == 0) {
        buffer->messageBody.userAdded.timestamp = hton64u((uint64_t) time(NULL));
    } else {
        buffer->messageBody.userAdded.timestamp = 0;
    }
    strncpy(buffer->messageBody.userAdded.name, username, strlen(username));
    buffer->messageHeader.length = htons(
            (uint16_t) (strlen(buffer->messageBody.userAdded.name) + sizeof(buffer->messageBody.userAdded.timestamp)));
    if ((bytesSend = send(sockfd, buffer, sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length), 0)) <
        0) {
        errnoPrint("error sending user added message");
        return -1;
    }
    if (bytesSend < (ssize_t) (ntohs(buffer->messageHeader.length) + sizeof(buffer->messageHeader))) {
        errnoPrint("sent too few bytes from user added message");
        return -1;
    }
    debugHexdump(buffer, sizeof(buffer->messageHeader.length) + sizeof(buffer->messageHeader.type) +
                         ntohs(buffer->messageHeader.length), "user added");
    return 1;
}

int sendUserRemoved(message *buffer, int sockfd, char *username, uint8_t code) {

    ssize_t bytesSend;
    memset(buffer->messageBody.userRemoved.name, 0, sizeof(buffer->messageBody.userRemoved.name));
    buffer->messageHeader.type = USER_REMOVED;
    buffer->messageBody.userRemoved.timestamp = hton64u(time(NULL));
    buffer->messageBody.userRemoved.code = code;
    strncpy(buffer->messageBody.userRemoved.name, username, strlen(username));
    buffer->messageHeader.length = htons(
            (uint16_t) (strlen(buffer->messageBody.userRemoved.name) +
                        sizeof(buffer->messageBody.userRemoved.timestamp) +
                        sizeof(buffer->messageBody.userRemoved.code)));
    debugPrint("user removed len = %d", ntohs(buffer->messageHeader.length));
    if (fcntl(sockfd, F_GETFD) == -1) {
        return -1;
    }
    if ((bytesSend = send(sockfd, buffer, sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length), 0)) <
        0) {
        if (errno == EBADFD) {
            return -1;
        }
        if (errno == EPIPE) {
            return -1;
        }
        if (errno == ECONNRESET) {
            return -1;
        }
        if(errno == ENOTSOCK){
            return -1;
        }
        errnoPrint("error sending user removed message %d", +errno);
        debugPrint("len = %d, bytesSend = %zi", ntohs(buffer->messageHeader.length), bytesSend);
        return -1;
    }
    if (bytesSend < (ssize_t) (ntohs(buffer->messageHeader.length) + sizeof(buffer->messageHeader))) {
        errnoPrint("sent too few bytes from user removed message");
        return -1;
    }

    debugPrint("sent: removeUser(%zi), bytesSend = %zi",
               sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length), bytesSend);
    debugHexdump(buffer, sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length), "user removed");
    return 1;
}

message *prepareServerMessage(message *buffer, char *username, int code, char *originalMessage) {

    switch (code) {
        case SERVER_CODE_INVALID_COMMAND:
            strncpy(buffer->messageBody.server2Client.text, notificationInvalidCommand,
                    strlen(notificationInvalidCommand));
            break;

        case SERVER_CODE_INVALID_PERMISSIONS:
            strncpy(buffer->messageBody.server2Client.text, notificationInvalidPermissions,
                    strlen(notificationInvalidPermissions));
            break;

        case SERVER_CODE_GENERAL_PROBLEMS:
            strncpy(buffer->messageBody.server2Client.text, notificationGeneralProblem,
                    strlen(notificationGeneralProblem));
            break;

        case SERVER_CODE_PAUSED:
            strncpy(buffer->messageBody.server2Client.text, notificationServerPaused, strlen(notificationServerPaused));
            break;

        case SERVER_CODE_RESUMED:
            strncpy(buffer->messageBody.server2Client.text, notificationServerResumed,
                    strlen(notificationServerResumed));
            break;

        case SERVER_CODE_CLIENT_MESSAGE:
            strncpy(buffer->messageBody.server2Client.text, originalMessage, strlen(originalMessage));
            break;
        case SERVER_CODE_DO_NOT_KICK_YOURSELF:
            strncpy(buffer->messageBody.server2Client.text, notificationDontKickYourself,
                    strlen(notificationDontKickYourself));
            break;
    }
    buffer->messageHeader.type = SERVER_2_CLIENT;
    buffer->messageBody.server2Client.timestamp = hton64u(time(NULL));
    strcpy(buffer->messageBody.server2Client.originalSender, username);
    buffer->messageHeader.length = htons(sizeof(buffer->messageBody.server2Client.timestamp) +
                                         sizeof(buffer->messageBody.server2Client.originalSender) +
                                         strlen(buffer->messageBody.server2Client.text));
    return buffer;
}


int sendSth(message *buffer, int sockfd) {
    ssize_t bytesSend;
    if ((bytesSend = send(sockfd, buffer, sizeof(messageHeader) + ntohs(buffer->messageHeader.length), 0)) < 0) {
        errnoPrint("error sending server 2 client message");
        return -1;
    }
    if (bytesSend < (ssize_t) (sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length))) {
        errnoPrint("sent too few bytes of server 2 client message");
        return -1;
    }
    return 1;
}

int sendServerMessage(message *buffer, int sockfd, char *username, int code, char *originalMessage) {
    switch (code) {
        case SERVER_CODE_INVALID_COMMAND:
            strncpy(buffer->messageBody.server2Client.text, notificationInvalidCommand,
                    strlen(notificationInvalidCommand));
            break;

        case SERVER_CODE_INVALID_PERMISSIONS:
            strncpy(buffer->messageBody.server2Client.text, notificationInvalidPermissions,
                    strlen(notificationInvalidPermissions));
            break;

        case SERVER_CODE_GENERAL_PROBLEMS:
            strncpy(buffer->messageBody.server2Client.text, notificationGeneralProblem,
                    strlen(notificationGeneralProblem));
            break;

        case SERVER_CODE_PAUSED:
            strncpy(buffer->messageBody.server2Client.text, notificationServerPaused, strlen(notificationServerPaused));
            break;

        case SERVER_CODE_CLIENT_MESSAGE:
            strncpy(buffer->messageBody.server2Client.text, originalMessage, strlen(originalMessage));
            break;

        case SERVER_CODE_DO_NOT_KICK_YOURSELF:
            strncpy(buffer->messageBody.server2Client.text, notificationDontKickYourself,
                    strlen(notificationDontKickYourself));
            break;

        case SERVER_CODE_ALREADY_PAUSED:
            strncpy(buffer->messageBody.server2Client.text, notificationAlreadyPaused,
                    strlen(notificationAlreadyPaused));
            break;

        case SERVER_CODE_CANNOT_RESUME:
            strncpy(buffer->messageBody.server2Client.text, notificationCannotResume, strlen(notificationCannotResume));
            break;
    }

    ssize_t bytesSend;
    buffer->messageHeader.type = SERVER_2_CLIENT;
    buffer->messageBody.server2Client.timestamp = hton64u(time(NULL));
    strcpy(buffer->messageBody.server2Client.originalSender, username);
    buffer->messageHeader.length = htons(sizeof(buffer->messageBody.server2Client.timestamp) +
                                         sizeof(buffer->messageBody.server2Client.originalSender) +
                                         strlen(buffer->messageBody.server2Client.text));
    if (validateLength__(SERVER_2_CLIENT_MIN_LENGTH, SERVER_2_CLIENT_MAX_LENGTH, ntohs(buffer->messageHeader.length)) ==
        -1) {
        errnoPrint("invalid length of server2 client");
        return -1;
    }
    if ((bytesSend = send(sockfd, buffer, sizeof(messageHeader) + ntohs(buffer->messageHeader.length), 0)) < 0) {
        errnoPrint("error sending server 2 client message");
        return -1;
    }
    if (bytesSend < (ssize_t) (sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length))) {
        errnoPrint("sent too few bytes of server 2 client message");
        return -1;
    }
    debugHexdump(buffer, sizeof(buffer->messageHeader) + ntohs(buffer->messageHeader.length), "server 2 client");
    return 1;
}






