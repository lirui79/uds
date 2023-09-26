/*****************************************************/
/*
**   Author: lirui
**   Date: 2023/09/20
**   File: INetConnect.h
**   Function:  Interface of Connect for user
**   History:
**       2023/09/20 create by lirui
**
**   Copy Right: lirui
**
*/
/*****************************************************/
#ifndef INTERFACE_NET_CONNECT_H
#define INTERFACE_NET_CONNECT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>



#ifdef __cplusplus
extern "C" {
#endif


typedef struct INetConnect {
    int  (*set)(struct INetConnect *con, const char *address, int sckid);

    int  (*open)(struct INetConnect *con, const char *address, int domain, int type, int protocol);

    int  (*write)(struct INetConnect *con, const unsigned char *data, unsigned int size);

    int  (*read)(struct INetConnect *con, unsigned char *data, unsigned int size);

    int  (*readable)(struct INetConnect *con);

    int  (*close)(struct INetConnect *con);

    const char* (*address)(struct INetConnect *con);

    int (*socketID)(struct INetConnect *con);

    int (*free)(struct INetConnect *con);
} INetConnect;

// type = 0 server  type > 0 client
struct INetConnect* allocateINetConnect();

#ifdef __cplusplus
}
#endif


#endif