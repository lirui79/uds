#include "INetServer.h"
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdatomic.h>

_Atomic unsigned int   sign_exit;

void HandleSignal(int signo) {
	unsigned int sign = 0;
	sign = atomic_load(&sign_exit);
    atomic_store(&sign_exit, sign + 1);
    printf("ctr + c times:%d\n", sign);
}

int main(int argc, char *argv[]) {
	struct INetServer* server = NULL;
	unsigned int sign = 0;
    atomic_init(&sign_exit, 0);
    // ctrl+c 捕获
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

	server = allocateINetServer();
    server->init(server, "/tmp/vpuserver");
    while(1) {
    	sign = atomic_load(&sign_exit);
    	if (sign > 0) {
    		break;
    	}
    	sleep(1);
    	printf("wait\n");
    }
    server->exit(server);
    server->free(server);
    printf("server exit\n");

    return 0;
}