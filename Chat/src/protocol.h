#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SEND_USER_ADDED_TYPE_NOTIFY 0
#define SEND_USER_ADDED_TYPE_UPDATE 1

#define LOGIN_REQUEST 0
#define LOGIN_RESPONSE 1
#define CLIENT_2_SERVER 2
#define SERVER_2_CLIENT 3
#define USER_ADDED 4
#define USER_REMOVED 5

#define LENGTH_MAX 36
#define LENGTH_MIN 6

#define USERNAME_MAX 31
#define USERNAME_MIN 1

#define SERVER_CODE_INVALID_COMMAND 0
#define SERVER_CODE_INVALID_PERMISSIONS 1
#define SERVER_CODE_GENERAL_PROBLEMS 2
#define SERVER_CODE_PAUSED 3
#define SERVER_CODE_RESUMED 5
#define SERVER_CODE_CLIENT_MESSAGE 4
#define SERVER_CODE_DO_NOT_KICK_YOURSELF 7
#define SERVER_CODE_ALREADY_PAUSED 8
#define SERVER_CODE_CANNOT_RESUME 9

#define SERVERNAME_MAX 31

#define MAGIC_LOGIN_REQUEST 0x0badf00d
#define MAGIC_LOGIN_RESPONSE 0xc001c001

#define LOGIN_RESPONSE_STATUS_SUCCESS 0
#define LOGIN_RESPONSE_STATUS_NAME_TAKEN 1
#define LOGIN_RESPONSE_STATUS_NAME_INVALID 2
#define LOGIN_RESPONSE_STATUS_PROTOCOL_VERSION_MISMATCH 3
#define LOGIN_RESPONSE_STATUS_OTHER_SERVER_ERROR 255

#define TEXT_MAX 512
#define SERVER_NAME "Server_group_12"
#define SERVER_2_CLIENT_MIN_LENGTH 40
#define SERVER_2_CLIENT_MAX_LENGTH 552

#define USER_REMOVED_STATUS_CONN_CLOSED_BY_CLIENT 0
#define USER_REMOVED_STATUS_KICKED_FROM_SERVER 1

#define VERSION 0

#pragma pack(1)
typedef struct messageHeader {
    uint8_t type;
    uint16_t length;
} messageHeader;

typedef struct loginRequest {
    uint32_t magic;
    uint8_t version;
    char name[USERNAME_MAX];
} loginRequest;

typedef struct loginResponse {
    uint32_t magic;
    uint8_t code;
    char serverName[SERVERNAME_MAX];
} loginResponse;

typedef struct client2Server {
    char text[TEXT_MAX];
} client2Server;

typedef struct server2Client {
    uint64_t timestamp;
    char originalSender[32];
    char text[TEXT_MAX];
} server2Client;

typedef struct userAdded {
    uint64_t timestamp;
    char name[USERNAME_MAX];
} userAdded;

typedef struct userRemoved {
    uint64_t timestamp;
    uint8_t code;
    char name[USERNAME_MAX];
} userRemoved;

typedef union messageBody {
    loginRequest loginRequest;
    loginResponse loginResponse;
    client2Server client2Server;
    server2Client server2Client;
    userAdded userAdded;
    userRemoved userRemoved;
} messageBody;

typedef struct message {
    messageHeader messageHeader;
    union messageBody messageBody;
} message;


#pragma pack(0)


ssize_t receiveHeader(messageHeader *buffer, int sockfd);

int receiveLoginRequest(message *buffer, int sockfd);

int sendLoginResponse(message *buffer, int sockfd, uint8_t code);

int sendUserRemoved(message *buffer, int sockfd, char *username, uint8_t code);

int sendUserAdded(message *buffer, int sockfd, char *username, uint8_t type);

int receiveClientMessage(message *buffer, int sockfd);

int sendServerMessage(message *buffer, int sockfd, char *username, int code, char *originalMessage);

message *prepareServerMessage(message *buffer, char *username, int code, char *originalMessage);

int sendSth(message *buffer, int sockfd);

#endif
