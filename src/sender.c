#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h> // for opening file

void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sockfd;
    struct sockaddr_in receiver_addr;
    char buffer[1024];
    FILE *file;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&receiver_addr, 0, sizeof(receiver_addr));

    // Filling server information
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(hostUDPport);
    receiver_addr.sin_addr.s_addr = inet_addr(hostname); // Convert IPv4 addresses from text to binary form
    if (inet_pton(AF_INET, hostname, &receiver_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // Open the file
    file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    printf("File opened successfully.\n");

    // Read and send the file in chunks
    unsigned long long int bytesSent = 0;
    while (bytesSent < bytesToTransfer && !feof(file)) {
        size_t toRead = sizeof(buffer);
        if (bytesToTransfer - bytesSent < toRead) {
            toRead = bytesToTransfer - bytesSent;
        }
        size_t read = fread(buffer, 1, toRead, file);
        if (sendto(sockfd, buffer, read, 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
            perror("sendto failed");
            break;
        }
        bytesSent += read;
        printf("Sent %zu bytes, total sent: %llu bytes.\n", read, bytesSent);
    }

    if (bytesSent >= bytesToTransfer) {
        printf("Specified amount of data sent successfully.\n");
    } else {
        printf("Reached end of file before sending specified amount of data.\n");
    }

    fclose(file);
    close(sockfd);
    printf("File and socket closed, exiting.\n");
}



int main(int argc, char** argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* hostname = argv[1];
    unsigned short int hostUDPport = (unsigned short int)atoi(argv[2]);
    char* filename = argv[3];
    unsigned long long int bytesToTransfer = atoll(argv[4]);

    rsend(hostname, hostUDPport, filename, bytesToTransfer);

    return EXIT_SUCCESS;
}
