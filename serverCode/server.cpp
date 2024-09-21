#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>  // For checking and creating directories
#include <openssl/md5.h>  // For MD5 checksum
#include <iomanip>         // For formatting output
#include <sstream>         // For converting MD5 to string
#include <openssl/evp.h>  // For EVP API
#include "serverUtils.h"
#include <atomic>
#include <vector>
#include <array>
#include "serverConstants.h"  // Include the constants

using namespace std;

int sockUDP; 
struct sockaddr_in serverAddr, clientAddr; 
socklen_t addr_len; 
socklen_t client_addr_len; 
std::vector<char> receiveFileBuffer;  
std::vector<char> receiveCount;  
std::atomic<bool> endFlag = false;  
bool g_start_saving = false; 


fileInfo thisFileInfo;
packet thisPacket;
packet lostPacketID;

void saveFileMetadata(const fileInfo& recvFileInfo) {
    if (recvFileInfo.packetNum > 0) {
        thisFileInfo.packetNum = recvFileInfo.packetNum;
        thisFileInfo.fileSize = recvFileInfo.fileSize;
    }
}

bool receiveFromSocket(int sock, void* buffer, size_t bufferSize, struct sockaddr_in& addr, socklen_t& addrLen, const std::string& errorMessage) {
    if (recvfrom(sock, buffer, bufferSize, 0, (struct sockaddr*)&addr, &addrLen) == -1) {
        std::cerr << errorMessage << std::endl;
        return false;  
    }
    return true; 
}

void recvFile() {

    struct fileInfo recvFileInfo;
    socklen_t clientAddr_len = sizeof(clientAddr);
    memset(&recvFileInfo, 0, sizeof(recvFileInfo));

    if (!receiveFromSocket(sockUDP, &recvFileInfo, sizeof(recvFileInfo), clientAddr, clientAddr_len, "Could not receive metadata")) {
        exit(1);
    }
    
    cout << "File metadata received" << endl;
    saveFileMetadata(recvFileInfo);
    
    cout<<"Number of packets expected from client = "<<recvFileInfo.packetNum<<endl;
    cout<<"Size of file = "<<recvFileInfo.fileSize<<endl;


    cout << "Receive Packets" << endl;

    // Modern C++: Resize vectors to allocate memory for the entire file and packet count
    receiveFileBuffer.resize(thisFileInfo.fileSize);
    receiveCount.resize(thisFileInfo.packetNum, 0);  // Initialized to 0 (no packets received)

    g_start_saving = false;

    while (thisPacket.packetID != -1) {
        memset(&thisPacket, 0, sizeof(thisPacket));

        if (!receiveFromSocket(sockUDP, &thisPacket, sizeof(thisPacket), clientAddr, client_addr_len, "Receive file info failed")) {
            exit(1);
        }
        
        // Skip packets that haven't started the file save process
        if (!g_start_saving && thisPacket.packetID == thisFileInfo.packetNum) continue;
        g_start_saving = true;

        if (thisPacket.packetID > 0 && thisPacket.packetID <= thisFileInfo.packetNum) {
            // Copy the packet data into the buffer
            std::copy(thisPacket.packetData.begin(), thisPacket.packetData.end(), receiveFileBuffer.begin() + ((thisPacket.packetID - 1) * PACKET_SIZE));
            receiveCount[thisPacket.packetID - 1] = 1;  // Mark packet as received
        }
    }

    //cout << "Finish 1st round receive file" << endl;
}

void sendLostInfo() {
    int retransmitPacketIndex = 0;

    while (true) {
        int lostPacketCount = 0;
        retransmitPacketIndex = 0;
        memset(&lostPacketID, 0, sizeof(lostPacketID));

        // Find and count the lost packets
        for (int i = 1; i <= thisFileInfo.packetNum; i++) {
            if (receiveCount[i - 1] == 0) {
                memcpy(lostPacketID.packetData.data() + 4 * retransmitPacketIndex, &i, 4);  // Store lost packet ID
                retransmitPacketIndex++;
                lostPacketCount++;

                // Send lost packet IDs when the packet is full
                if (lostPacketCount % (PACKET_SIZE / 4) == 0) {
                    lostPacketID.packetID = 0;
                    if (sendto(sockUDP, &lostPacketID, sizeof(lostPacketID), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
                        perror("Failed to send lost packet info");
                        exit(1);
                    }
                    usleep(PACKET_SPACE);
                    memset(&lostPacketID, 0, sizeof(lostPacketID));
                    retransmitPacketIndex = 0;
                }
            }
        }
        
        cout << "# Lost Packets: " << lostPacketCount << endl;

        if (lostPacketCount == 0) {
            endFlag = true;
            break;
        }

        // Send remaining lost packet IDs which don't fill a full packet
        if (retransmitPacketIndex > 0) {
            if (sendto(sockUDP, &lostPacketID, sizeof(lostPacketID), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
                perror("Failed to send last lost packet ID");
                exit(1);
            }
            usleep(PACKET_SPACE);
        }
    }
}

void* recvRetransmit(void* data) {
    cout << "Receive resent packets from client" << endl;
    int endRetransmit = 0;

    while (endRetransmit == 0) {
        memset(&thisPacket, 0, sizeof(thisPacket));

        if (!receiveFromSocket(sockUDP, &thisPacket, sizeof(thisPacket), clientAddr, client_addr_len, "Receive retransmit failed")) {
            exit(1);
        }
        
        if (thisPacket.packetID > 0 && thisPacket.packetID <= thisFileInfo.packetNum) {
            std::copy(thisPacket.packetData.begin(), thisPacket.packetData.end(), receiveFileBuffer.begin() + ((thisPacket.packetID - 1) * PACKET_SIZE));
            receiveCount[thisPacket.packetID - 1] = 1;  // Mark retransmitted packet as received
        }

        endRetransmit = endFlag.load();
    }

    return NULL;
}

void requestRetransmit() {
    cout << "Request packets lost during transmission" << endl;

    endFlag = false;
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, recvRetransmit, NULL);
    
    sendLostInfo();

    memset(&lostPacketID, 0, sizeof(lostPacketID));
    lostPacketID.packetID = -2;
    for (int j = 0; j < 20; j++) {
        if (sendto(sockUDP, &lostPacketID, sizeof(lostPacketID), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
            perror("Failed to send end retransmit signal");
            exit(1);
        }
        usleep(PACKET_SPACE);
    }

    cout << "Lost packets received" << endl;
}

int main(int argc, char* argv[]) {
    int port = stoi(argv[1]);
    
    createUDPSocket(port, sockUDP, serverAddr, addr_len);
    recvFile();
    requestRetransmit();
    close(sockUDP);
    
    writeFile("testserver/data.bin", receiveFileBuffer.data(), thisFileInfo.fileSize);

    cout << "File Received" << endl;
    return 0;
}

