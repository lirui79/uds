#include "IMutex.h"
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>


typedef struct IMutex_t {
	struct IMutex    imutex;
	pthread_mutex_t  mutex;
} IMutex_t;

static int IMutex_default_lock(struct IMutex *mx) {
	struct IMutex_t *im = (struct IMutex_t *)mx;
	return pthread_mutex_lock(&(im->mutex));
}

static int IMutex_default_unlock(struct IMutex *mx) {
	struct IMutex_t *im = (struct IMutex_t *)mx;
	return pthread_mutex_unlock(&(im->mutex));
}

static int IMutex_default_free(struct IMutex *mx) {
	struct IMutex_t *im = (struct IMutex_t *)mx;
    pthread_mutex_destroy(&(im->mutex));
    free(im);
    return 0;
}

#if defined(__cplusplus)
extern "C" {
#endif

struct IMutex* allocateIMutex() {
	struct IMutex_t *im = (struct IMutex_t *) malloc(sizeof(struct IMutex_t));
	struct IMutex   *mx     = (struct IMutex*) im;
	if (im == NULL) {
		return NULL;
	}
	memset(im, 0, sizeof(struct IMutex_t));
	if (0 != pthread_mutex_init(&(im->mutex), NULL)) {
		free(im);
		return NULL;
	}
	mx->lock     = IMutex_default_lock;
	mx->unlock   = IMutex_default_unlock;
	mx->free     = IMutex_default_free;
	return mx;
}

#if defined(__cplusplus)
}
#endif


typedef struct IEvent_t {
	struct IEvent  evnt;
    int            sign;
    pthread_cond_t cond;
} IEvent_t;

static void IEvent_default_waitTime(unsigned long ms, struct timespec* now)    {
        while(clock_gettime(CLOCK_REALTIME, now) != 0) {
            ; // -lrt 库
        }
        ms += (unsigned long)(now->tv_nsec / 1000 /1000);
        now->tv_sec += ( ms / 1000) ;
        ms = (ms % 1000) ;
        now->tv_nsec = ((long long)ms) * 1000 * 1000;
}

static int IEvent_default_wait(struct IEvent *ie, struct IMutex *ix, unsigned long ts) {
	struct IEvent_t *im = (struct IEvent_t *)ie;
	struct IMutex_t *mx = (struct IMutex_t *)ix;
    int wait = 1 ;
    if (im->sign == 1) {
       wait = 1 ;
    } else {
        struct timespec now;
        IEvent_default_waitTime(ts, &now);
        if (0 == pthread_cond_timedwait(&(im->cond), &(mx->mutex), &now)) {
        	wait = 1;
        } else {
        	wait = 0;
        }
    }
    im->sign = 0 ;
    return wait ;
}

static int IEvent_default_wakeOne(struct IEvent *ie) {
	struct IEvent_t *im = (struct IEvent_t *)ie;
	im->sign = 1;
    return pthread_cond_signal(&(im->cond));
}

static int IEvent_default_wakeAll(struct IEvent *ie) {
	struct IEvent_t *im = (struct IEvent_t *)ie;
	im->sign = 1;
    return pthread_cond_broadcast(&(im->cond));
}

static int IEvent_default_free(struct IEvent *ie) {
	struct IEvent_t *im = (struct IEvent_t *)ie;
	im->sign = 0 ;
    pthread_cond_destroy(&(im->cond));
    free(im);
    return 0;
}

#if defined(__cplusplus)
extern "C" {
#endif

struct IEvent* allocateIEvent() {
	struct IEvent_t *im = (struct IEvent_t *) malloc(sizeof(struct IEvent_t));
	struct IEvent   *mx     = (struct IEvent*) im;
	if (im == NULL) {
		return NULL;
	}
	memset(im, 0, sizeof(struct IEvent_t));
	if (0 != pthread_cond_init(&(im->cond), NULL)) {
		free(im);
		return NULL;
	}
	mx->wait      = IEvent_default_wait;
	mx->wakeOne   = IEvent_default_wakeOne;
	mx->wakeAll   = IEvent_default_wakeAll;
	mx->free      = IEvent_default_free;
	return mx;
}

#if defined(__cplusplus)
}
#endif