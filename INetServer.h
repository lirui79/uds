/*****************************************************/
/*
**   Author: lirui
**   Date: 2023/09/20
**   File: INetServer.h
**   Function:  Interface of Server for user
**   History:
**       2023/09/20 create by lirui
**
**   Copy Right: lirui
**
*/
/*****************************************************/
#ifndef INTERFACE_NET_SERVER_H
#define INTERFACE_NET_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct INetServer {
    int (*init)(struct INetServer *srv, const char *address);

    int (*exit)(struct INetServer *srv);

    int (*free)(struct INetServer *srv);
} INetServer;


struct INetServer* allocateINetServer();

#ifdef __cplusplus
}
#endif

#endif