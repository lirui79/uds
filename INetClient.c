#include "INetClient.h"
#include "INetConnect.h"
#include "IMutex.h"
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


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
    int                    epollfd;
} INetClient_t;


static int setnonblocking(int fd) { //将fd设置称非阻塞
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int sockfd, void *evnt) { //向Epoll中添加fd
//oneshot表示是否设置称同一时刻，只能有一个线程访问fd，数据的读取都在主线程中，所以调用都设置成false
    struct epoll_event event;
    if (evnt == NULL) {
       event.data.fd = sockfd;
    } else {
        event.data.ptr = evnt;
    }
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event); //添加fd
    setnonblocking(sockfd);
}

static void *NetClient_threadRecv_select(void *argv) {
    fd_set rfds;
    struct timeval timeout={1,0};
    struct INetClient_t *client = (struct INetClient_t*) argv;
	struct IMutex       *mutex     = client->mutex;
    struct IEvent       *event     = client->event;
    struct INetConnect  *iconn     = client->iconn;
    unsigned int status = 0;
    int maxFd = -1, code = -1;
    int           bufsize      = 4096;

    while(1) {
    	status = atomic_load(&(client->status));
    	if (status != 0x2) {
    		break;
    	}
    	mutex->lock(mutex);
    	maxFd = iconn->socketID(iconn);
    	mutex->unlock(mutex);
        if (maxFd == -1) {
            code = iconn->open(iconn, client->address, AF_UNIX, SOCK_STREAM, 0);
            if (code < 0) {
                mutex->lock(mutex);
                event->wait(event, mutex, 5000);
                mutex->unlock(mutex);
            }
            continue;
        }
    	FD_ZERO(&rfds);
		FD_SET(maxFd, &rfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        code = select(maxFd + 1, &rfds, NULL, NULL, &timeout);
        if (code <= 0) {
            printf("client select\n");
            continue;
        }
        bufsize = iconn->readable(iconn);
        if (bufsize <= 0) {
            iconn->close(iconn);
        	continue;
        }
        if (bufsize > 4096)
            bufsize = 4096;
        memset(client->data, 0, bufsize);
		code = iconn->read(iconn, client->data, bufsize);// dispatch data 
        if (code < 0) {
			printf("client recv code %d\n", code);
            iconn->close(iconn);
			continue;
        }
        //process data
    	mutex->lock(mutex);
    	client->size = bufsize;
    	event->wakeOne(event);
    	mutex->unlock(mutex);
    }

    atomic_init(&(client->status), 0);
    return NULL;
}

static void *NetClient_threadRecv(void *argv) {
    struct INetClient_t *client = (struct INetClient_t*) argv;
    struct IMutex       *mutex     = client->mutex;
    struct IEvent       *event     = client->event;
    struct INetConnect  *iconn     = client->iconn;
    struct INetConnect  *iecon     = NULL;
    unsigned int status = 0;
    int     code = -1, i = 0, readSize = 0, evsize = 2, ewsize = 0;
    int           bufsize      = 4096;
    struct epoll_event   *events = NULL;
    client->epollfd = epoll_create(evsize);
    addfd(client->epollfd, iconn->socketID(iconn), iconn);
    events = (struct epoll_event   *)malloc(evsize * sizeof(struct epoll_event));
    memset(events, 0, evsize * sizeof(struct epoll_event));
    while(1) {
        status = atomic_load(&(client->status));
        if (status != 0x2) {
            break;
        }

        if (iconn->socketID(iconn) == -1) {
            code = iconn->open(iconn, client->address, AF_UNIX, SOCK_STREAM, 0);
            if (code < 0) {
                mutex->lock(mutex);
                event->wait(event, mutex, 5000);
                mutex->unlock(mutex);
                continue;
            }
            addfd(client->epollfd, iconn->socketID(iconn), iconn);
        }

        ewsize = epoll_wait(client->epollfd, events, evsize, 1000);
        if (ewsize < 0) {
            printf("epoll_wait error %d\n", ewsize);
            break;
        }

        for (i = 0; i < ewsize; ++i) {
            if(events[i].events & EPOLLIN) {
                iecon = (struct INetConnect  *)events[i].data.ptr;
                readSize = iecon->readable(iecon);
                if (readSize <= 0) {
                    struct epoll_event ev;
                    ev.data.ptr = (void*) iecon ;
                    ev.events =  EPOLLIN;
                    epoll_ctl(client->epollfd , EPOLL_CTL_DEL , iecon->socketID(iecon) , &ev);
                    iecon->close(iecon);
                    continue;
                }
                if (readSize > 4096)
                    readSize = 4096;
                memset(client->data, 0, readSize);
                bufsize = iecon->read(iecon, client->data, readSize);// dispatch data 
                if (bufsize < 0) {
                    printf("client recv code %d\n", bufsize);
                    struct epoll_event ev;
                    ev.data.ptr = (void*) iecon ;
                    ev.events =  EPOLLIN;
                    epoll_ctl(client->epollfd , EPOLL_CTL_DEL , iecon->socketID(iecon) , &ev);
                    iecon->close(iecon);
                    continue;
                }
                //process data
                mutex->lock(mutex);
                client->size = bufsize;
                event->wakeOne(event);
                mutex->unlock(mutex);
            }
        }
    }

    {
        struct epoll_event ev;
        ev.data.ptr =  (void*) iconn ;
        ev.events   =  EPOLLIN;
        epoll_ctl(client->epollfd , EPOLL_CTL_DEL , iconn->socketID(iconn) , &ev);
    }
    close(client->epollfd);
    client->epollfd = -1;
    if (events != NULL)
        free(events);
    events = NULL;
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
    if (iconn->write(iconn, data, size) != size) {
    	return -1;
    }
    mutex->lock(mutex);
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
    event->wakeAll(event);
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

    iconn->close(iconn);
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
