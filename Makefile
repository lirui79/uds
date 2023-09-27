
all:server client libwebnet.so demo-server demo-client

list.o:
	gcc -c list.c -I./ -o list.o

IMutex.o:
	gcc -c IMutex.c -I./ -lpthread -o IMutex.o

INetConnect.o:
	gcc -c INetConnect.c -I./ -o INetConnect.o

INetClient.o:
	gcc -c INetClient.c -I./ -lpthread -o INetClient.o

INetServer.o:
	gcc -c INetServer.c -I./ -lpthread -o INetServer.o


libwebnet.so:list.o IMutex.o INetConnect.o INetClient.o INetServer.o
	gcc -g list.c IMutex.c INetConnect.c INetClient.c INetServer.c -I./ -lpthread -fPIC -shared  -o libwebnet.so

server:server.c
	gcc -g list.c IMutex.c INetConnect.c INetServer.c  server.c -I./ -lpthread -o server

client:client.c
	gcc -g list.c IMutex.c INetConnect.c INetClient.c client.c -I./ -lpthread -o client

demo-server:server.c
	gcc -g server.c -L. -lwebnet -I./ -lpthread -o demo-server

demo-client:client.c
	gcc -g client.c -L. -lwebnet  -I./ -lpthread -o demo-client


clean:
	rm *.o *.so *server *client
