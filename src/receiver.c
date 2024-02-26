#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024  // Adjust based on expected data packet size

struct ack_packet {
    unsigned int seq_num; // Sequence number of the acknowledged packet
};

void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    int sockfd;
    struct sockaddr_in myAddr, senderAddr;
    char buffer[BUFFER_SIZE];
    FILE *file;
    unsigned long long int bytesWritten = 0;
    unsigned int expectedSeqNum = 0; // Expected sequence number initialization
    socklen_t senderAddrLen = sizeof(senderAddr);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = INADDR_ANY;
    myAddr.sin_port = htons(myUDPport);
    if (bind(sockfd, (struct sockaddr *) &myAddr, sizeof(myAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Open file for writing
    file = fopen(destinationFile, "wb");
    if (!file) {
        perror("File opening failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&senderAddr, &senderAddrLen);
        if (recvLen < 0) {
            perror("Receive failed");
            break;
        }

        // Assuming the sequence number is the first 4 bytes of the packet
        if (recvLen >= sizeof(unsigned int)) {
            unsigned int seqNum;
            memcpy(&seqNum, buffer, sizeof(unsigned int));
            seqNum = ntohl(seqNum); // Convert from network byte order to host byte order

            struct ack_packet ack;
            ack.seq_num = htonl(seqNum); // Always send ACK for received packet

            // Only write data if it's the expected sequence number
            if (seqNum == expectedSeqNum) {
                fwrite(buffer + sizeof(unsigned int), 1, recvLen - sizeof(unsigned int), file);
                bytesWritten += recvLen - sizeof(unsigned int);
                expectedSeqNum++; // Increment expected sequence number for the next packet
            } else {
                // If not the expected sequence number, do not write data
                // This implicitly requests retransmission of lost packets
            }

            // Send ACK for the received packet
            if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&senderAddr, senderAddrLen) < 0) {
                perror("ACK sendto failed");
            }
        }
    }

    fclose(file); // Close the file
    close(sockfd); // Close the socket
}

int main(int argc, char** argv) {
    // This is a skeleton of a main function.
    // You should implement this function more completely
    // so that one can invoke the file transfer from the
    // command line.

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);
}