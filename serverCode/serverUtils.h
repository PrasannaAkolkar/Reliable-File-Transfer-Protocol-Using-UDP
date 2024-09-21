#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <netinet/in.h>  // For sockaddr_in and socklen_t
#include <sys/socket.h>  // For socket functions
#include <sys/types.h>   // For socket types
#include <sys/stat.h>    // For file system operations
#include <array>
#include "serverConstants.h"


// Declare utility functions
void createUDPSocket(int &port, int &sockUDP, struct sockaddr_in &serverAddr, socklen_t &addr_len);
void writeFile(const char* path, const char* buffer, const size_t buffer_size);
std::string calculateMD5(const std::string &filePath);
void error(const char *msg);

// Structs shared between the client and server
struct fileInfo {
    int packetNum;
    int fileSize;
};

struct packet {
    int packetID;
    std::array<char, PACKET_SIZE> packetData;  // Fixed-size array for packet data
};

#endif // UTILS_H

