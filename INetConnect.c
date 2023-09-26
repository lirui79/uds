#include "INetConnect.h"
#include "IMutex.h"
#include <sys/types.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct INetConnect_t {
    struct INetConnect      iconn;
	int                     sockID;
    struct IMutex          *mutex;
	char                    address[256];
	char                    remote[256];
} INetConnect_t;

static int INetConnect_default_set(struct INetConnect *con, const char *address, int sckid) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
    iconn->mutex->lock(iconn->mutex);
	iconn->sockID = sckid;
	sprintf(iconn->address, "%s", address);
    iconn->mutex->unlock(iconn->mutex);
	return 0;
}

static int INetConnect_default_open(struct INetConnect *con, const char *address, int domain, int type, int protocol) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
    int sckid = socket(domain, type, protocol);
    struct  sockaddr_un  servAddr, clntAddr;
    int size = 0;
    memset(&clntAddr, 0, sizeof(struct  sockaddr_un));
    clntAddr.sun_family = domain;
    strcpy(clntAddr.sun_path, iconn->address);
    size = offsetof(struct sockaddr_un, sun_path) + strlen(clntAddr.sun_path);
    unlink(iconn->address);
    if (bind(sckid, (struct sockaddr *)&clntAddr, size) < 0) {
        close(sckid);
        return -1;
    }

    memset(&servAddr, 0, sizeof(struct  sockaddr_un));
    servAddr.sun_family = domain;
    strcpy(servAddr.sun_path, address);
    size = offsetof(struct sockaddr_un, sun_path) + strlen(servAddr.sun_path);
    if (connect(sckid, (struct sockaddr *)&servAddr, size) < 0){
        close(sckid);
        sckid = -1;
        return -2;
    }

    iconn->mutex->lock(iconn->mutex);
    iconn->sockID = sckid;
	sprintf(iconn->remote, "%s", address);
    iconn->mutex->unlock(iconn->mutex);
    return 0;
}

static int INetConnect_default_write(struct INetConnect *con, const unsigned char *data, unsigned int size) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
    int length = size, sz = 0;
	mutex->lock(mutex);
    if (iconn->sockID < 0) {
	     mutex->unlock(mutex);
         return -1;
    }

    while(length > 0) {
        sz = send(iconn->sockID, data, length, MSG_NOSIGNAL|MSG_WAITALL);
	    if (sz <= 0) {
            if (errno == EINTR)
                sz = 0;
            else if (errno == EAGAIN)
                sz = 0;
            else {
	            mutex->unlock(mutex);
                return -2;
            }
		}
        length -= sz;
        data += sz;
    }
	mutex->unlock(mutex);
    return (size - length);
}

static int INetConnect_default_read(struct INetConnect *con, unsigned char *data, unsigned int size) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
    int length = size, sz = 0;
	mutex->lock(mutex);
    if (iconn->sockID < 0) {
	     mutex->unlock(mutex);
         return -1;
    }

    while(length > 0) {
        sz = recv(iconn->sockID, data, length, MSG_NOSIGNAL|MSG_WAITALL) ;
        if (sz <= 0) {
            if (errno == EINTR)
                sz = 0;
            else if (errno == EAGAIN)
                sz = 0;
            else {
	            mutex->unlock(mutex);
                return -2;
            }
        }
        length -= sz;
        data += sz;
    }
	mutex->unlock(mutex);
    return (size - length) ;
}

static int INetConnect_default_readable(struct INetConnect *con) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
    int    can_readSize = 0;
	mutex->lock(mutex);
    if (iconn->sockID < 0) {
	    mutex->unlock(mutex);
        return -1 ;
    }
    if (ioctl(iconn->sockID , FIONREAD , &can_readSize) != 0) {
	    mutex->unlock(mutex);
        return -2 ;
    }
	mutex->unlock(mutex);
    return can_readSize;
}

static int INetConnect_default_close(struct INetConnect *con) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
	mutex->lock(mutex);
    if (iconn->sockID < 0) {
	     mutex->unlock(mutex);
         return -1 ;
    }
    close(iconn->sockID);
    iconn->sockID = -1;
	mutex->unlock(mutex);
    return 0;
}

static const char* INetConnect_default_address(struct INetConnect *con) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	return iconn->address;
}

static int INetConnect_default_socketID(struct INetConnect *con) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
	int  sockID = -1;
	mutex->lock(mutex);
	sockID = iconn->sockID;
	mutex->unlock(mutex);
    return sockID;
}

static int INetConnect_default_free(struct INetConnect *con) {
	struct INetConnect_t *iconn = (struct INetConnect_t*) con;
	struct IMutex* mutex = iconn->mutex;
	mutex->lock(mutex);
    if (iconn->sockID < 0) {
	     mutex->unlock(mutex);
	     mutex->free(mutex);
	     free(iconn);
         return 1;
    }
    close(iconn->sockID);
    iconn->sockID = -1;
	mutex->unlock(mutex);
	mutex->free(mutex);
	free(iconn);
	return 0;
}

#if defined(__cplusplus)
extern "C" {
#endif

struct INetConnect* allocateINetConnect() {
	struct INetConnect_t *iconn = (struct INetConnect_t *) malloc(sizeof(struct INetConnect_t));
	struct INetConnect   *con     = (struct INetConnect*) iconn;
	if (iconn == NULL) {
		return NULL;
	}
	memset(iconn, 0, sizeof(struct INetConnect_t));
	iconn->sockID= -1;
	iconn->mutex = allocateIMutex();
	con->set       = INetConnect_default_set;
	con->open      = INetConnect_default_open;
	con->write     = INetConnect_default_write;
	con->read      = INetConnect_default_read;
	con->readable  = INetConnect_default_readable;
	con->close     = INetConnect_default_close;
	con->address   = INetConnect_default_address;
	con->socketID  = INetConnect_default_socketID;
	con->free      = INetConnect_default_free;
	return con;
}

#if defined(__cplusplus)
}
#endif
