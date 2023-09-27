#include "INetServer.h"
#include "INetConnect.h"
#include "IMutex.h"
#include "list.h"
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

typedef struct INetConItem{
	struct INetConnect  *iconn;
	struct list_head     entry;
	unsigned long        procid;
} INetConItem;

typedef struct INetDataItem {
    struct INetConnect  * iconn;
    struct list_head      entry;
    unsigned long         procid;
	unsigned char         buffer[4096];
	int                   bufsize;
} INetDataItem;

typedef struct INetServer_t {
    struct INetServer      server;
    _Atomic unsigned long  procid;
    _Atomic unsigned int   status;// 0-exit  1-initing  2-running 3-exiting
    struct IMutex         *mutex;
    struct IEvent         *event;
    struct IMutex         *hlock;
    struct list_head       conn_head;
    struct IMutex         *dlock;
    struct list_head       conn_data;
    int                    sckID;
    int                    epollfd;
	char                   address[256];
	pthread_t              tids[5];
} INetServer_t;

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

static void INetServer_clear(struct INetServer_t *server) {
	struct IMutex       *mutex     = server->mutex;
    struct IEvent       *event     = server->event;
    struct list_head    *lhead     = &server->conn_head;
    struct IMutex       *hlock     = server->hlock;
    struct list_head    *ldata     = &server->conn_data;
    struct IMutex       *dlock     = server->dlock;
    struct INetConItem  *citem     = NULL;
    struct INetDataItem *ditem     = NULL;
    struct list_head    *node      = NULL, *next = NULL;
    int    sckID = -1;
    hlock->lock(hlock);
    lhead     = &server->conn_head;
    node      = lhead->next;
    next      = node->next;
    while(node != lhead) {
        list_del(node);
        citem = list_entry(node, INetConItem, entry);
        sckID = citem->iconn->socketID(citem->iconn);
        if (sckID >= 0) {
			struct epoll_event ev;
	        ev.data.ptr = (void*) citem ;
	        ev.events =  EPOLLIN;
	        epoll_ctl(server->epollfd , EPOLL_CTL_DEL , sckID , &ev);
	        citem->iconn->close(citem->iconn);
	    }
        citem->iconn->free(citem->iconn);
        free(citem);
        node = next;
        next = node->next;
    }
    hlock->unlock(hlock);

    dlock->lock(dlock);
    ldata     = &server->conn_data;
    node      = ldata->next;
    next      = node->next;
    while(node != ldata) {
        list_del(node);
        ditem = list_entry(node, INetDataItem, entry);
        free(ditem);
        node = next;
        next = node->next;
    }
    dlock->unlock(dlock);
}

static void *INetServer_threadRecv(void *argv) {
    struct INetServer_t *server    = (struct INetServer_t*) argv;
	struct IMutex       *mutex     = server->mutex;
    struct IEvent       *event     = server->event;
    struct list_head    *lhead     = &server->conn_head;
    struct IMutex       *hlock     = server->hlock;
    struct list_head    *ldata     = &server->conn_data;
    struct IMutex       *dlock     = server->dlock;
    struct INetConItem  *citem     = NULL;
    struct INetDataItem *ditem     = NULL;
    unsigned int status = 0;
    unsigned long   procid = 0;
    int code = -1, sckID = -1, i = 0, readSize = 0, evsize = 1024, ewsize = 0;
    struct epoll_event   *events   = NULL;

    server->epollfd = epoll_create(evsize);
    addfd(server->epollfd, server->sckID, NULL);
    events = (struct epoll_event   *)malloc(evsize * sizeof(struct epoll_event));
    memset(events, 0, evsize * sizeof(struct epoll_event));

    while(1) {
    	status = atomic_load(&(server->status));
    	if (status != 0x2) {
    		break;
    	}

    	ewsize = epoll_wait(server->epollfd, events, evsize, 1000);
    	if (ewsize < 0) {
    		printf("epoll_wait error %d\n", ewsize);
    		break;
    	}

        for (i = 0; i < ewsize; ++i) {
        	if (events[i].data.fd == server->sckID) {
			    struct sockaddr_in remote_addr;
			    socklen_t sock_len = sizeof(struct sockaddr_un);
			    struct epoll_event ev;
	        	sckID = accept(server->sckID,(struct sockaddr *)&remote_addr, &sock_len);
	        	citem = (struct INetConItem*)malloc(sizeof(struct INetConItem));
	        	citem->iconn = allocateINetConnect();
	        	citem->iconn->set(citem->iconn, inet_ntoa(remote_addr.sin_addr), sckID);
                printf("create conn %d %s\n", citem->iconn->socketID(citem->iconn), citem->iconn->address(citem->iconn));
	        	procid = atomic_load(&(server->procid));
	        	atomic_store(&(server->procid), procid + 1);
	        	citem->procid = procid;
	    	    hlock->lock(hlock);
	    	    list_add_tail(&citem->entry, lhead);
	    	    hlock->unlock(hlock);
	    	    ev.data.ptr = (void*) citem;
                ev.events =  EPOLLIN | EPOLLET;
	    	    epoll_ctl(server->epollfd , EPOLL_CTL_ADD , sckID , &ev);
	    	    continue;
        	} 

        	if(events[i].events & EPOLLIN) {
        		citem = (struct INetConItem*)events[i].data.ptr;
                readSize = citem->iconn->readable(citem->iconn);
                if (readSize <= 0) {
                	struct epoll_event ev;
	    	        ev.data.ptr = (void*) citem ;
                    ev.events =  EPOLLIN;
                    sckID = citem->iconn->socketID(citem->iconn);
                    printf("close conn %d %s\n", citem->iconn->socketID(citem->iconn), citem->iconn->address(citem->iconn));
                    epoll_ctl(server->epollfd , EPOLL_CTL_DEL , sckID , &ev);
                    citem->iconn->close(citem->iconn);
                    citem->iconn->free(citem->iconn);
                    hlock->lock(hlock);
                    list_del(&citem->entry);
                    hlock->unlock(hlock);
                    free(citem);
                	continue;
                }
                ditem = (struct INetDataItem*)malloc(sizeof(struct INetDataItem));
                memset(ditem, 0, sizeof(struct INetDataItem));
                ditem->iconn   = citem->iconn;
                ditem->procid  = citem->procid;
                ditem->bufsize = citem->iconn->read(citem->iconn, ditem->buffer, readSize);
                printf("read %d %s:%d %d\n", citem->iconn->socketID(citem->iconn), citem->iconn->address(citem->iconn), ditem->bufsize, readSize);
                dlock->lock(dlock);
	    	    list_add_tail(&ditem->entry, ldata);
                dlock->unlock(dlock);
                mutex->lock(mutex);
                event->wakeOne(event);
                mutex->unlock(mutex);
        	}
        }
    }

    INetServer_clear(server);
    {
    	struct epoll_event ev;
        ev.data.fd = server->sckID;
        ev.events =  EPOLLIN;
        epoll_ctl(server->epollfd , EPOLL_CTL_DEL , server->sckID , &ev);
    }
    close(server->epollfd);
    server->epollfd = -1;
    close(server->sckID);
    if (events != NULL)
        free(events);
    events = NULL;
    atomic_init(&(server->status), 0);
    return NULL;
}


static void *INetServer_threadProc(void *argv) {
    struct INetServer_t *server    = (struct INetServer_t*) argv;
	struct IMutex       *mutex     = server->mutex;
    struct IEvent       *event     = server->event;
    struct IMutex       *dlock     = server->dlock;
    struct list_head    *ldata     = &server->conn_data;
    struct list_head    *node      = NULL, *next = NULL;
    struct INetDataItem *ditem     = NULL;
    unsigned int         status    = 0, bufsize = 0;
    int                  code      = 0, send = 0;
    char                 buffer[4096] = {0};
    while(1) {
    	status = atomic_load(&(server->status));
    	if (status != 0x2) {
    		break;
    	}
    	dlock->lock(dlock);
        ldata = &server->conn_data;
        node  = ldata->next;
        next  = node->next;
    	if (node == ldata) {
            dlock->unlock(dlock);
	        mutex->lock(mutex);
	        code = event->wait(event, mutex, 1000);
	        mutex->unlock(mutex);
	        continue;
	    } else {
            list_del(node);
            ditem = list_entry(node, INetDataItem, entry);
            node = next;
            next = node->next;
            dlock->unlock(dlock);
            memset(buffer, 0, 4096);
            printf("process %d %s:%s %d\n", ditem->iconn->socketID(ditem->iconn), ditem->iconn->address(ditem->iconn), ditem->buffer, ditem->bufsize);
            bufsize = sprintf(buffer, "back to client:%s",ditem->buffer);
            ditem->iconn->write(ditem->iconn, buffer, bufsize);
            free(ditem);
        }
    }

    return NULL;
}


static int INetServer_default_init(struct INetServer *srv, const char *address) {
	struct INetServer_t   *server    = (struct INetServer_t*) srv;
	struct IMutex         *mutex     = server->mutex;
    struct IEvent         *event     = server->event;
    unsigned int status = atomic_load(&(server->status));
    struct sockaddr_un servAddr;
    int code = 0, size = 0, sckID = -1, i = 0;
    if (status != 0) {
    	return status;
    }

    atomic_store(&(server->status), 1);
	if ((sckID = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
		sckID = -1;
    	atomic_init(&(server->status), 0);
    	return -1;
	}
    memset(&servAddr, 0, sizeof(struct sockaddr_un));  
    servAddr.sun_family = AF_UNIX;  
    strcpy(servAddr.sun_path, address);  
    size = offsetof(struct sockaddr_un, sun_path) + strlen(servAddr.sun_path);  
    unlink(address);  
    if (bind(sckID, (struct sockaddr *)&servAddr, size) < 0) {  
        printf("bind error\n");
        close(sckID);
		sckID = -1;
    	atomic_init(&(server->status), 0);
    	return -2;
    }  
    printf("UNIX domain socket bound\n");  
      
    if (listen(sckID, 20) < 0) {  
        printf("listen error\n");
        close(sckID);
		sckID = -1;
    	atomic_init(&(server->status), 0);
    	return -3;
    }  

    server->mutex->lock(server->mutex);
    server->sckID = sckID;
	sprintf(server->address, "%s", address);
    server->mutex->unlock(server->mutex);
    atomic_store(&(server->status), 2);
    code = pthread_create(&(server->tids[0]), NULL, INetServer_threadRecv, (void*) server);
    for (i = 1; i < 5; ++i) {
		code += pthread_create(&(server->tids[i]), NULL, INetServer_threadProc, (void*) server);
    }
    return 0;
}

static int INetServer_default_exit(struct INetServer *srv) {
	struct INetServer_t   *server    = (struct INetServer_t*) srv;
	struct IMutex         *mutex     = server->mutex;
    struct IEvent         *event     = server->event;
	struct IMutex         *hlock     = server->hlock;
	struct IMutex         *dlock     = server->dlock;
    unsigned int status = atomic_load(&(server->status));
    unsigned int wait                = 0;
    int                    i         = 0;
    atomic_store(&(server->status), 3);
    mutex->lock(mutex);
    event->wakeAll(event);
    mutex->unlock(mutex);
    for (i = 0;i < 5; ++i) {
    	pthread_join(server->tids[i], NULL);
    }
    while(wait < 20) {
    	status  = atomic_load(&(server->status));
    	if (status == 0) {
    		break;
    	}
    	++wait;
    	usleep(500);
    }

	return 0;
}

static int INetServer_default_free(struct INetServer *srv) {
	struct INetServer_t   *server    = (struct INetServer_t*) srv;
	struct IMutex         *mutex     = server->mutex;
    struct IEvent         *event     = server->event;
	struct IMutex         *hlock     = server->hlock;
	struct IMutex         *dlock     = server->dlock;
	mutex->free(mutex);
	event->free(event);
	hlock->free(hlock);
	dlock->free(dlock);
	free(server);
	return 0;
}


#if defined(__cplusplus)
extern "C" {
#endif

struct INetServer* allocateINetServer() {
	struct INetServer_t *server  = (struct INetServer_t *) malloc(sizeof(struct INetServer_t));
	struct INetServer   *srv     = (struct INetServer*) server;
	if (server == NULL) {
		return NULL;
	}
	memset(server, 0, sizeof(struct INetServer_t));
	atomic_init(&(server->procid), 0);
	atomic_init(&(server->status), 0);
	server->mutex    = allocateIMutex();
	server->event    = allocateIEvent();
	server->hlock    = allocateIMutex();
	server->dlock    = allocateIMutex();
	server->sckID    = -1;
	INIT_LIST_HEAD(&server->conn_head);
	INIT_LIST_HEAD(&server->conn_data);
	srv->init        = INetServer_default_init;
	srv->exit        = INetServer_default_exit;
	srv->free        = INetServer_default_free;
	return srv;
}

#if defined(__cplusplus)
}
#endif

