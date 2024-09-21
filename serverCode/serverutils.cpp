// utils.cpp
#include "serverUtils.h"
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>  // For EVP API
#include <openssl/md5.h>  // For MD5 checksum
#include <cstdlib>


//using namespace Constants;

// Error handling utility
void error(const char *msg) {
    perror(msg);
    exit(0);
}

// Socket creation utility
void createUDPSocket(int &port, int &sockUDP, struct sockaddr_in &serverAddr, socklen_t &addr_len) {
    sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockUDP < 0) {
        error("Create UDP socket");
        exit(1);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    addr_len = sizeof(serverAddr);

    if (::bind(sockUDP, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
        error("Bind UDP socket");
        exit(1);
    }

    std::cout << "UDP socket created on port: " << port << std::endl;
}

// Function to calculate MD5 checksum of a file using EVP API
std::string calculateMD5(const std::string &filePath) {
    unsigned char mdValue[EVP_MAX_MD_SIZE];
    unsigned int mdLen;
    std::ifstream file(filePath, std::ifstream::binary);

    if (!file) {
        std::cerr << "Failed to open file for MD5 calculation: " << filePath << std::endl;
        return "";
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "Failed to create EVP_MD_CTX" << std::endl;
        return "";
    }

    const EVP_MD *md = EVP_md5();  // Specify MD5
    EVP_DigestInit_ex(mdctx, md, NULL);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer))) {
        EVP_DigestUpdate(mdctx, buffer, file.gcount());
    }

    // Process any remaining bytes in the last read
    if (file.gcount() > 0) {
        EVP_DigestUpdate(mdctx, buffer, file.gcount());
    }

    EVP_DigestFinal_ex(mdctx, mdValue, &mdLen);
    EVP_MD_CTX_free(mdctx);  // Clean up

    // Convert the MD5 hash to a readable hexadecimal string
    std::stringstream md5String;
    for (unsigned int i = 0; i < mdLen; i++) {
        md5String << std::hex << std::setw(2) << std::setfill('0') << (int)mdValue[i];
    }

    return md5String.str();
}

// Utility function to write the file and calculate MD5
void writeFile(const char* path, const char* buffer, const size_t buffer_size) {
    // Check if the directory exists, if not, create it
    struct stat st = {0};
    const char* dirPath = "testserver";

    // Check if the testserver directory exists
    if (stat(dirPath, &st) == -1) {
        // Create the /testserver directory
        if (mkdir(dirPath, 0777) == -1) {
            perror("Error creating testserver directory");
            exit(1);
        }
        std::cout << "Directory testserver created" << std::endl;
    }

    // Full file path
    std::string fullFilePath = std::string(dirPath) + "/data.bin";


    // Open and write to the file
    std::ofstream file;
    try {
        file.open(fullFilePath, std::ofstream::binary | std::ios::trunc);
    } catch (const std::ofstream::failure& e) {
        std::cerr << "file_write_failed!" << std::endl;
        return;
    }

    file.write(buffer, buffer_size);
    file.close();
    std::cout << "File saved as: " << fullFilePath << std::endl;

    // Calculate MD5 checksum for the file and print it
    std::string md5sum = calculateMD5(fullFilePath);
    if (!md5sum.empty()) {
        std::cout << "MD5 checksum of the file: " << md5sum << std::endl;
    }
}

