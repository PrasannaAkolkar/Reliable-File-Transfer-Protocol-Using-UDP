// structs.h
#ifndef STRUCTS_H
#define STRUCTS_H
#include "clientConstants.h"
#include <vector>
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h> // For socket functions

using namespace Constants;

// File information structure
struct fileInfo {
    int packetNum;
    int fileSize;
};

// Packet structure
struct packet {
    int packetID;
    char packetData[PACKET_SIZE];
};

// Thread data structure
struct ThreadData {
    int threadID;
    int startPacket;
    int endPacket;
    std::vector<char> *buff;  // Pointer to buffer vector to avoid copying
};

#endif // STRUCTS_H

