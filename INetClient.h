/*****************************************************/
/*
**   Author: lirui
**   Date: 2023/09/20
**   File: INetClient.h
**   Function:  Interface of Client for user
**   History:
**       2023/09/20 create by lirui
**
**   Copy Right: lirui
**
*/
/*****************************************************/
#ifndef INTERFACE_NET_CLIENT_H
#define INTERFACE_NET_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct INetClient {
    int (*init)(struct INetClient *clt, const char *address);

    int (*write)(struct INetClient *clt, unsigned char *data, unsigned int size, unsigned long ts);

    int (*exit)(struct INetClient *clt);

    int (*free)(struct INetClient *clt);
} INetClient;


struct INetClient* allocateINetClient(const char *address);


#ifdef __cplusplus
}
#endif

#endif