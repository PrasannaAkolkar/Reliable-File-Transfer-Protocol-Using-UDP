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
#include <new>
#include <fstream>
#include <sys/time.h>
#include <algorithm>  
#include <vector>
#include <chrono>
#include "clientStructs.h"
#include "clientConstants.h"
#include "clientUtils.h"

using namespace std;
using namespace Constants;


int sockUDP; 
struct sockaddr_in serverAddr; 
socklen_t addr_len; 

std::vector<char> fileBuffer;  
std::vector<char> retransmitInfo; 
std::atomic<bool> endFlag = false;
fileInfo thisFileInfo;
packet thisPacket;
packet retransmitPacket;

const int NUM_THREADS1 = 4

// Struct for passing data to retransmission threads
struct RetransmitThreadData {
    int startIndex;  // Start index in retransmitInfo
    int endIndex;    // End index in retransmitInfo
};

// Struct for passing data to file transmission threads

void *sendFilePart(void *threadarg) {
    struct ThreadData *data = (struct ThreadData *) threadarg;

    int startPacket = data->startPacket;
    int endPacket = data->endPacket;
    int sentCount = (startPacket - 1) * PACKET_SIZE;
    
    int duplicatePacketFrequency = 1000;

    struct packet thisPacket;

    for (int i = startPacket; i <= endPacket; i++) {
        memset(&thisPacket, 0, sizeof(thisPacket));

        // Handle last packet which may not be full PACKET_SIZE
        if (i == thisFileInfo.packetNum) {  
            int remainingBytes = thisFileInfo.fileSize - sentCount;
            std::copy(data->buff->begin() + sentCount, data->buff->begin() + sentCount + remainingBytes, thisPacket.packetData);
        } else { 
            std::copy(data->buff->begin() + sentCount, data->buff->begin() + sentCount + PACKET_SIZE, thisPacket.packetData);
        }
        
        thisPacket.packetID = i;

        // Send the packet
        if (sendto(sockUDP, &thisPacket, sizeof(thisPacket), 0, (const struct sockaddr *)&serverAddr, addr_len) == -1) {
            std::cerr << "Packet: " << thisPacket.packetID << " send error" << std::endl;
        }
        usleep(DELAY); 
        
        // Duplicate packet transmission
        if (i % duplicatePacketFrequency == 0) {
            if (sendto(sockUDP, &thisPacket, sizeof(thisPacket), 0, (const struct sockaddr *)&serverAddr, addr_len) == -1) {
                std::cerr << "Duplicate packet: " << thisPacket.packetID << " send error" << std::endl;
            }
        }

        sentCount += PACKET_SIZE;
        usleep(DELAY);  // Prevent network congestion
    }

    pthread_exit(NULL);
}

void sendFile() {

    for (int i = 0; i < 20; i++) {
      if (sendto(sockUDP, &thisFileInfo, sizeof(thisFileInfo), 0, (const struct sockaddr *)&serverAddr, addr_len) == -1) {
          error("sending file info");
          exit(1);
       }
        usleep(DELAY);
    }
    
    cout << "File metadata sent" << endl;
     
    cout << "Start sending file with " <<NUM_THREADS<<" threads" << std::endl;

    pthread_t threads[NUM_THREADS];
    struct ThreadData threadData[NUM_THREADS];
    int packetsPerThread = thisFileInfo.packetNum / NUM_THREADS;

    for (int t = 0; t < NUM_THREADS; t++) {
        threadData[t].threadID = t;
        threadData[t].startPacket = t * packetsPerThread + 1;
        threadData[t].endPacket = (t == NUM_THREADS - 1) ? thisFileInfo.packetNum : (t + 1) * packetsPerThread;
        threadData[t].buff = &fileBuffer;  // Pass a pointer to the buffer

        int rc = pthread_create(&threads[t], NULL, sendFilePart, (void *)&threadData[t]);
        if (rc) {
            std::cerr << "Error: unable to create thread " << rc << std::endl;
            exit(-1);
        }
    }

    // Join threads after they are done sending
    for (int t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    std::cout << "Finished sending file with multiple threads" << std::endl;

    // Send signal to indicate end of first round
    struct packet endPacketSignal;
    for (int i = 0; i <= 20; i++) {
        memset(&endPacketSignal, 0, sizeof(endPacketSignal));
        endPacketSignal.packetID = -1;

        if (sendto(sockUDP, &endPacketSignal, sizeof(endPacketSignal), 0, (const struct sockaddr *)&serverAddr, addr_len) == -1) {
            std::cerr << "End first round signal error" << std::endl;
        }

        usleep(DELAY);
    }
    
}

void receiveLostPacketIDsMainThread() {
    while (1) {
        memset(&thisPacket, 0, sizeof(thisPacket));
        if (recvfrom(sockUDP, &thisPacket, sizeof(thisPacket), 0, NULL, NULL) == -1) {
            error("Error receiving lost packet ids");
            exit(1);
        }
        if (thisPacket.packetID == -2) {
            endFlag = true;
            break;
        }
        
        std::copy(thisPacket.packetData, thisPacket.packetData + PACKET_SIZE, retransmitInfo.begin());
    }

    // Start multiple threads to retransmit lost packets
    int numLostPackets = retransmitInfo.size() / 4;  
    int packetsPerThread = numLostPackets / NUM_THREADS1;

    pthread_t threads[NUM_THREADS1];
    RetransmitThreadData threadData[NUM_THREADS1];

    for (int t = 0; t < NUM_THREADS1; t++) {
        threadData[t].startIndex = t * packetsPerThread * 4;  // Multiply by 4 to get byte index
        threadData[t].endIndex = (t == NUM_THREADS1 - 1) ? retransmitInfo.size() : (t + 1) * packetsPerThread * 4;

        int rc = pthread_create(&threads[t], NULL, resendLostPacketsUsingOtherThread, (void *)&threadData[t]);
        if (rc) {
            std::cerr << "Error: Unable to create retransmission thread " << rc << std::endl;
            exit(-1);
        }
    }

    // Join the retransmission threads after they finish
    for (int t = 0; t < NUM_THREADS1; t++) {
        pthread_join(threads[t], NULL);
    }

    std::cout << "Finished retransmitting lost packets with multiple threads" << std::endl;
}

void *resendLostPacketsUsingOtherThread(void *data) {
    RetransmitThreadData *threadData = (RetransmitThreadData *) data;
    int retransmitID;

    for (int i = threadData->startIndex; i < threadData->endIndex; i += 4) {  // Iterate over 4-byte packet IDs
        memcpy(&retransmitID, retransmitInfo.data() + i, 4);
        if ((retransmitID > 0) && (retransmitID <= thisFileInfo.packetNum)) {
            memset(&retransmitPacket, 0, sizeof(retransmitPacket));
            std::copy(fileBuffer.begin() + (retransmitID - 1) * PACKET_SIZE,
                      fileBuffer.begin() + retransmitID * PACKET_SIZE,
                      retransmitPacket.packetData);
            retransmitPacket.packetID = retransmitID;

            if (sendto(sockUDP, &retransmitPacket, sizeof(retransmitPacket), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
                std::cerr << "Error: Failed to retransmit lost packet" << std::endl;
                pthread_exit(NULL);  // Exit on error
            }

            usleep(DELAY);
        }
    }
    
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // Start timer using chrono
    auto start = std::chrono::high_resolution_clock::now();

    readFile(argv[1], fileBuffer, thisFileInfo);

    int serverPort = stoi(argv[3]);
   
    createSocket(argv[2], serverPort, sockUDP, serverAddr, addr_len);

    sendFile();

    cout << "Resend lost packets" << endl;

    retransmitInfo.resize(PACKET_SIZE, 0);
    receiveLostPacketIDsMainThread();

    close(sockUDP);

    // End timer using chrono
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;  // Calculate elapsed time in seconds

    double fileSizeMb = (thisFileInfo.fileSize / 1000000.0) * 8;  // File size in Megabits
    double throughput = fileSizeMb / elapsed.count();  // Throughput in Mbits/s

    cout << "File Send to the server" << endl;
    cout << "Transmission Time = " << elapsed.count() << " sec" << endl;
    cout << "Throughput = " << throughput << " Mbits/s" << endl;

    return 0;
}
