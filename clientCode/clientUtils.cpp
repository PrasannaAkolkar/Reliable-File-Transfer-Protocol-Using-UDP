#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <cstring>
#include "clientUtils.h"

using namespace std;

void error(const char *msg) {
    perror(msg);
    exit(0);
}

void createSocket(const char* address, int &port, int &sockUDP, struct sockaddr_in &serverAddr, socklen_t &addr_len) {
    sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockUDP < 0) {
        error("UDP Socket could not be created");
        exit(1);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(address);
    serverAddr.sin_port = htons(port);
    addr_len = sizeof(serverAddr);

    cout << "UDP socket created" << endl;
}

void readFile(const char* path, std::vector<char>& buff, fileInfo& thisFileInfo) {
    FILE *read_file;
    if ((read_file = fopen(path, "rb")) == NULL) {
        printf("Error opening specified file.\n");
        exit(1);
    }
    
    fseek(read_file, 0, SEEK_END);
    thisFileInfo.fileSize = (int)ftell(read_file);
    fseek(read_file, 0, SEEK_SET);
    
    thisFileInfo.packetNum = thisFileInfo.fileSize / PACKET_SIZE;
    if ((thisFileInfo.fileSize % PACKET_SIZE) != 0) {
        thisFileInfo.packetNum++;
    }

    buff.resize(thisFileInfo.fileSize);
    fread(buff.data(), 1, thisFileInfo.fileSize, read_file);
    fclose(read_file);

    //cout << "Total file size = " << thisFileInfo.fileSize << endl;
    //cout << "Total packet number = " << thisFileInfo.packetNum << endl;
}


