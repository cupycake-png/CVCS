#ifndef NETWORKUTILS_H
#define NETWORKUTILS_H

#include <string>

std::string receiveMessage(int socketFD);
int sendMessage(int socketFD, std::string message);

#endif