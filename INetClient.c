#include "INetClient.h"
#include "INetConnect.h"
#include "IMutex.h"
#include <string.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>


typedef struct INetClient_t {
    struct INetClient      client;
    _Atomic unsigned int   status;// 0-exit  1-initing  2-running 3-exiting
    struct IMutex         *mutex;
    struct IEvent         *event;
    struct INetConnect    *iconn;
	char                   address[256];
	pthread_t              tids[1];
	unsigned char          data[4096];
	int                    size;
} INetClient_t;

static void *NetClient_threadRecv(void *argv) {
    fd_set rfds;
    struct timeval timeout={1,0};
    struct sockaddr_in remote_addr;
    struct INetClient_t *client = (struct INetClient_t*) argv;
	struct IMutex       *mutex     = client->mutex;
    struct IEvent       *event     = client->event;
    struct INetConnect  *iconn     = client->iconn;
    unsigned int status = 0;
    int maxFd = -1, code = -1;
    unsigned char buffer[4096] = {0};
    int           bufsize      = 4096;
    while(1) {
    	status = atomic_load(&(client->status));
    	maxFd = -1;
    	if (status != 0x2) {
    		break;
    	}
    	mutex->lock(mutex);
    	maxFd = iconn->socketID(iconn);
    	mutex->unlock(mutex);
    	FD_ZERO(&rfds);
		FD_SET(maxFd, &rfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        code = select(maxFd + 1, &rfds, NULL, NULL, &timeout);
        if (code <= 0) {
            printf("client select\n");
            continue;
        }
        bufsize = 4096;
        bufsize = iconn->readable(iconn);
        if (bufsize <= 0) {
        	continue;
        }
 		code = iconn->read(iconn, buffer, bufsize);// dispatch data 
        if (code < 0) {
			printf("client recv code %d\n", code);
			continue;
        }
        //process data
    	mutex->lock(mutex);
    	client->size = bufsize;
    	memcpy(client->data, buffer, bufsize);
    	event->wakeOne(event);
    	mutex->unlock(mutex);
    }

    atomic_init(&(client->status), 0);
    return NULL;
}

static int INetClient_default_init(struct INetClient *clt, const char *address) {
	struct INetClient_t *client = (struct INetClient_t*) clt;
	struct IMutex         *mutex     = client->mutex;
    struct IEvent         *event     = client->event;
    struct INetConnect    *iconn     = client->iconn;
    unsigned int status = atomic_load(&(client->status));
    int code = 0;
    if (status != 0) {
    	return status;
    }
    atomic_store(&(client->status), 1);
    code = iconn->open(iconn, address, AF_UNIX, SOCK_STREAM, 0);
    if (code < 0) {
    	atomic_init(&(client->status), 0);
    	return code;
    }
	sprintf(client->address, "%s", address);
    atomic_store(&(client->status), 2);
    code = pthread_create(&(client->tids[0]), NULL, NetClient_threadRecv, (void*) client);
    return 0;
}

static int INetClient_default_write(struct INetClient *clt, unsigned char *data, unsigned int size, unsigned long ts) {
	struct INetClient_t *client = (struct INetClient_t*) clt;
	struct IMutex         *mutex     = client->mutex;
    struct IEvent         *event     = client->event;
    struct INetConnect    *iconn     = client->iconn;

    mutex->lock(mutex);
    if (iconn->write(iconn, data, size) != size) {
    	mutex->unlock(mutex);
    	return -1;
    }
    if (event->wait(event, mutex, ts) == 0) {
    	mutex->unlock(mutex);
    	printf("wait %d seconds timeout\n", ts);
    	return -2;
    }
    size = client->size;
    memcpy(data, client->data, size);
    mutex->unlock(mutex);
	return size;
}

static int INetClient_default_exit(struct INetClient *clt) {
	struct INetClient_t *client = (struct INetClient_t*) clt;
	struct IMutex       *mutex     = client->mutex;
    struct IEvent       *event     = client->event;
    struct INetConnect  *iconn     = client->iconn;
    unsigned int status            = atomic_load(&(client->status));
    unsigned int wait              = 0;
    atomic_store(&(client->status), 3);
    mutex->lock(mutex);
    event->wakeOne(event);
    mutex->unlock(mutex);
    pthread_join(client->tids[0], NULL);
    while(wait < 20) {
    	status  = atomic_load(&(client->status));
    	if (status == 0) {
    		break;
    	}
    	++wait;
    	usleep(500);
    }
    mutex->lock(mutex);
    iconn->close(iconn);
    mutex->unlock(mutex);
	return 0;
}

static int INetClient_default_free(struct INetClient *clt) {
	struct INetClient_t *client = (struct INetClient_t*) clt;
	struct IMutex         *mutex     = client->mutex;
    struct IEvent         *event     = client->event;
    struct INetConnect    *iconn     = client->iconn;
	mutex->free(mutex);
	event->free(event);
	iconn->free(iconn);
	free(client);
	return 0;
}

#if defined(__cplusplus)
extern "C" {
#endif

struct INetClient* allocateINetClient(const char *address) {
	struct INetClient_t *client  = (struct INetClient_t *) malloc(sizeof(struct INetClient_t));
	struct INetClient   *clt     = (struct INetClient*) client;
	if (client == NULL) {
		return NULL;
	}
	memset(client, 0, sizeof(struct INetClient_t));
	atomic_init(&(client->status), 0);
	client->mutex    = allocateIMutex();
	client->event    = allocateIEvent();
	client->iconn    = allocateINetConnect();
	clt->init        = INetClient_default_init;
	clt->write       = INetClient_default_write;
	clt->exit        = INetClient_default_exit;
	clt->free        = INetClient_default_free;
	client->iconn->set(client->iconn, address, -1);
	return clt;
}


#if defined(__cplusplus)
}
#endif
