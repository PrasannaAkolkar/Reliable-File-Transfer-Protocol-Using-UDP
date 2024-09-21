// clientUtils.h
#ifndef CLIENTUTILS_H
#define CLIENTUTILS_H

#include <vector>
#include "clientStructs.h"

// Declare utility functions

void error(const char *msg);

void createSocket(const char* address, int &port, int &sockUDP, struct sockaddr_in &serverAddr, socklen_t &addr_len);

void readFile(const char* path, std::vector<char>& buff, fileInfo& thisFileInfo);

#endif // CLIENTUTILS_H
