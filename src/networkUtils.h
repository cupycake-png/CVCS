#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    using SocketType = SOCKET;
#else
    using SocketType = int;
#endif

// For windows only
void initialiseSockets();
void cleanupSockets();

SocketType createSocket(int domain, int type, int protocol);
void closeSocket(SocketType socket);

SocketType connectToServer(std::string serverAddress, int port);

ssize_t recvAll(SocketType socket, void* buffer, size_t length);
ssize_t sendAll(SocketType socket, void* buffer, size_t length);

std::string receiveMessage(SocketType socket);
int sendMessage(SocketType socket, std::string message);

#endif