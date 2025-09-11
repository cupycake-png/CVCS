#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include "networkUtils.h"

ssize_t recvAll(int sock, void* buffer, size_t length){
    char* buff = static_cast<char*>(buffer);
    size_t total = 0;

    while(total < length){
        ssize_t n = recv(sock, buff + total, length - total, 0);
        
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

ssize_t sendAll(int socketFD, void* buffer, size_t length){
    char* buff = static_cast<char*>(buffer);
    size_t total = 0;

    while(total < length){
        ssize_t n = send(socketFD, buff + total, length - total, 0);
        
        if(n <= 0){
            throw std::runtime_error("send() failed");
        }

        total += n;
    }

    return total;
}

std::string receiveMessage(int socketFD){
    uint32_t tmp;
    recvAll(socketFD, &tmp, sizeof(tmp));

    uint32_t messageLength = ntohl(tmp);
    std::string message(messageLength, '\0');

    if(messageLength > 0){
        recvAll(socketFD, &message[0], messageLength);
    }

    return message;
}

int sendMessage(int socketFD, std::string message){
    uint32_t messageLength = htonl(message.size());
    sendAll(socketFD, &messageLength, sizeof(messageLength));

    if(!message.empty()){
        sendAll(socketFD, message.data(), message.size());
    }

    return 0;
}