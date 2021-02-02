#include "connectionhandler.h"
#include "clientthread.h"
#include "util.h"
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "user.h"
#include <stdlib.h>
#include <errno.h>

static int createPassiveSocket(in_port_t port) {
    int fileDescriptor = -1;
    struct sockaddr_in sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr.sin_port = htons(port);
    fileDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    // to avoid the delay between closing and restarting connectionhandler
    if (setsockopt(fileDescriptor, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        return -1;
    }
    if (bind(fileDescriptor, (struct sockaddr *) &sockaddr, (socklen_t) sizeof(sockaddr)) < 0) {
        infoPrint("Could not open socket on port %d", ntohs((int) sockaddr.sin_port));
        return -1;
    }
    if (listen(fileDescriptor, SOMAXCONN) < 0) {
        errnoPrint("listen() failed");
        return -1;
    } else {
        infoPrint("Listening on port %d", ntohs((int) sockaddr.sin_port));
    }
    return fileDescriptor;
}

int connectionHandler(in_port_t port) {
    User *userToThread;
    int socketFileDescriptor;
    char str[INET_ADDRSTRLEN];
    struct sockaddr_in socketAdress;

    const int fileDescriptor = createPassiveSocket(port);
    if (fileDescriptor == -1) {
        return -1;
    }

    memset(&socketAdress, 0, sizeof(socketAdress));
    socketAdress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAdress.sin_family = AF_INET;
    socketAdress.sin_port = htons(port);
    socklen_t addr_size = sizeof(socketAdress);

    for (;;) {
        if ((socketFileDescriptor = accept(fileDescriptor, (struct sockaddr *) &socketAdress, &addr_size)) < 0) {
            errnoPrint("connection on port %d", (int) socketAdress.sin_port);

        } else {

            inet_ntop(AF_INET, &socketAdress.sin_addr, str, INET_ADDRSTRLEN);
            infoPrint("Client %s connected successfully on socket: %d!", str, socketFileDescriptor);
            userToThread = malloc(sizeof(User));
            userToThread->socketFileDescriptor = socketFileDescriptor;
            if (pthread_create(&userToThread->thread, NULL, clientthread, userToThread) < 0) {
                errnoPrint("pthread_create(clientthread...)");
            }
        }

    }
}
