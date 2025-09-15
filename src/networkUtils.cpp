#include <stdexcept>
#include <cstdint>
#include "networkUtils.h"

#ifdef _WIN32
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSESOCKET closesocket
#else
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSESOCKET close
#endif

void initialiseSockets(){
    #ifdef _WIN32
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
            throw std::runtime_error("WSAStartup failed");
        }
    #endif
}

void cleanupSockets(){
    #ifdef _WIN32
        WSACleanup();
    #endif
}

SocketType createSocket(int domain=AF_INET, int type=SOCK_STREAM, int protocol=0){
    SocketType retSocket = socket(domain, type, protocol);

    return retSocket;
}

void closeSocket(SocketType socket){
    CLOSESOCKET(socket);
}

SocketType connectToServer(std::string serverAddress, int port){
    SocketType retSocket = createSocket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if(inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0){
        closeSocket(retSocket);

        throw std::runtime_error("Invalid server address: " + serverAddress);
    }

    connect(retSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));

    return retSocket;
}

ssize_t recvAll(SocketType socket, void* buffer, size_t length){
    char* buff = static_cast<char*>(buffer);
    size_t total = 0;

    while(total < length){
        #ifdef _WIN32
            ssize_t n = recv(socket, buff + total, static_cast<int>(length - total), 0);
        #else
            ssize_t n = recv(socket, buff + total, length - total, 0);
        #endif

        if(n == 0){
            throw std::runtime_error("Connection closed by peer");
        }
        
        if(n < 0){
            throw std::runtime_error("recv() failed");
        }

        total += n;
    }

    return total;
}

ssize_t sendAll(SocketType socket, void* buffer, size_t length){
    char* buff = static_cast<char*>(buffer);
    size_t total = 0;

    while(total < length){
        #ifdef _WIN32
            ssize_t n = send(socket, buff + total, static_cast<int>(length - total), 0);
        #else
            ssize_t n = send(socket, buff + total, length - total, 0);
        #endif

        if(n <= 0){
            throw std::runtime_error("send() failed");
        }

        total += n;
    }

    return total;
}

std::string receiveMessage(SocketType socket){
    uint32_t tmp;
    recvAll(socket, &tmp, sizeof(tmp));

    uint32_t messageLength = ntohl(tmp);
    std::string message(messageLength, '\0');

    if(messageLength > 0){
        recvAll(socket, &message[0], messageLength);
    }

    return message;
}

int sendMessage(SocketType socket, std::string message){
    uint32_t messageLength = htonl(message.size());
    sendAll(socket, &messageLength, sizeof(messageLength));

    if(!message.empty()){
        sendAll(socket, message.data(), message.size());
    }

    return 0;
}