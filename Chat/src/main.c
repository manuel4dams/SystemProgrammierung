#include <stdlib.h>
#include "connectionhandler.h"
#include "broadcastagent.h"
#include "util.h"
#include <limits.h>

int main(int argc, char **argv) {
    int result = 0;
    char *endptr = NULL;
    debugEnable();
    styleEnable();
    setProgName(argv[0]);


    if (argc <= 1) {
        if (broadcastAgentStart() == -1) {
            return EXIT_FAILURE;
        }
        infoPrint("Chat server, group 12");
        result = connectionHandler((in_port_t) 8111);
    } else if (argc == 2) {
        if (broadcastAgentStart() == -1) {
            return EXIT_FAILURE;
        }
        //check if long too long
        long port = strtol(argv[1], &endptr, 10);
        debugPrint("port %ld | endptr = %s", port, endptr);
        if (port > UINT16_MAX) {
            infoPrint("Port number too big!");
            return EXIT_FAILURE;
        }
        if (!*endptr) {
            if ((result = connectionHandler((in_port_t) port) == -1)) {
                debugPrint("could not open socket on port %ld", port);
                return EXIT_FAILURE;
            }
            infoPrint("Chat server, group 12");
        } else {
            infoPrint("Invalid Port! Exiting..");
            return EXIT_FAILURE;
        }
    } else if (argc < 1 || argc >= 2) {
        infoPrint("No valid parameters given. Usage : ./server [PORT]");
    }
    return result != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
