/*****************************************************/
/*
**   Author: lirui
**   Date: 2023/09/20
**   File: IMutex.h
**   Function:  Interface of mutex for user
**   History:
**       2023/09/20 create by lirui
**
**   Copy Right: lirui
**
*/
/*****************************************************/
#ifndef INTERFACE_MUTEX_LOCK_H
#define INTERFACE_MUTEX_LOCK_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IMutex {
    int (*lock)(struct IMutex *im);

    int (*unlock)(struct IMutex *im);

    int (*free)(struct IMutex *im);
} IMutex;

struct IMutex* allocateIMutex();

typedef struct IEvent {
    int (*wait)(struct IEvent *ie, struct IMutex *ix, unsigned long ts);

    int (*wakeOne)(struct IEvent *ie);

    int (*wakeAll)(struct IEvent *ie);

    int (*free)(struct IEvent *ie);
} IEvent;

struct IEvent* allocateIEvent();

#ifdef __cplusplus
}
#endif

#endif
