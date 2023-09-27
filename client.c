#include "INetClient.h"
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
	struct INetClient* client = NULL;
	unsigned int sign = 0;
	char data[4096] = {0};
    int size = 0, send = 0;
    if (argc < 2) {
        printf("please: ./client /tmp/vpu-test-0");
        return -1;
    }
    atomic_init(&sign_exit, 0);
    // ctrl+c 捕获
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

	client = allocateINetClient(argv[1]);
    client->init(client, "/tmp/vpuserver");
    while(1) {
    	sign = atomic_load(&sign_exit);
    	if (sign > 0) {
    		break;
    	}
        memset(data, 0, 4096);
        size = sprintf(data, "hello world client->server send times:%d", send++);
        size = client->write(client, data, size, 5000);
        if (size <= 0) {
            printf("client write to server error %d\n", size);
            sleep(2);
        } else {
           printf("recv from server %d:%s\n", size, data);
           sleep(1);
        }
    }
    client->exit(client);
    client->free(client);
    printf("client exit\n");

    return 0;
}