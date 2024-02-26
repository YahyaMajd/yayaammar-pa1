#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> // Include this header

#define BUFFER_SIZE 1024
#define HEADER_SIZE 8
#define DATA_SIZE (BUFFER_SIZE - HEADER_SIZE)
#define ACK_TIMEOUT 2  // Timeout in seconds for ACKs

struct packet {
    unsigned int seq_num;
    char data[DATA_SIZE];
    int acked;  // 1 if ACK received, 0 otherwise
    time_t send_time;  // Time when the packet was last sent
};

struct ack_packet {
    unsigned int seq_num;
};

void resend_packets(struct packet packets[], int sockfd, struct sockaddr_in *receiverAddr, int *outstanding_packets) {
    time_t current_time = time(NULL);

    for (int i = 0; i < *outstanding_packets; i++) {
        // Check if the packet was not acknowledged and if the resend timeout has passed
        if (!packets[i].acked && difftime(current_time, packets[i].send_time) > ACK_TIMEOUT) {
            char buffer[BUFFER_SIZE];
            *((unsigned int *)buffer) = htonl(packets[i].seq_num);
            memcpy(buffer + HEADER_SIZE, packets[i].data, DATA_SIZE);

            if (sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)receiverAddr, sizeof(*receiverAddr)) < 0) {
                perror("Sendto failed during retransmission");
                continue;  // Try to send next packets
            }

            packets[i].send_time = current_time;  // Update send time for the retransmitted packet
            printf("Packet %u retransmitted.\n", packets[i].seq_num);
        }
    }
}


void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sockfd;
    struct sockaddr_in receiverAddr;
    FILE *file;
    unsigned long long int bytesSent = 0;
    unsigned int seqNum = 0;
    char buffer[BUFFER_SIZE];
    struct timeval tv;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket timeout for receiving ACKs
    tv.tv_sec = ACK_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Setup receiver address
    memset(&receiverAddr, 0, sizeof(receiverAddr));
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(hostUDPport);
    if (inet_pton(AF_INET, hostname, &receiverAddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Open file
    file = fopen(filename, "rb");
    if (!file) {
        perror("File opening failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (bytesSent < bytesToTransfer) {
        // Check if there is data to read and send
        if (fread(buffer + HEADER_SIZE, 1, DATA_SIZE, file) > 0) {
            ssize_t packetSize = DATA_SIZE + HEADER_SIZE;
            *((unsigned int*)buffer) = htonl(seqNum); // Sequence number in network byte order

            // Transmit the packet
            if (sendto(sockfd, buffer, packetSize, 0, (struct sockaddr*)&receiverAddr, sizeof(receiverAddr)) < 0) {
                perror("Sendto failed");
                break;
            }

            struct ack_packet ack;
            socklen_t fromlen = sizeof(receiverAddr);

            // Attempt to receive ACK
            if (recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&receiverAddr, &fromlen) < 0) {
                if (errno == EWOULDBLOCK) {
                    printf("ACK timeout, need to retransmit\n");
                    fseek(file, -DATA_SIZE, SEEK_CUR); // Move file pointer back to re-read and re-send the same chunk
                    continue; // Attempt to resend the packet
                } else {
                    perror("recvfrom failed");
                    break;
                }
            }

            // Convert ack.seq_num from network byte order to host byte order before comparison
            ack.seq_num = ntohl(ack.seq_num);
            if (ack.seq_num == seqNum) {
                // ACK received correctly, proceed to next packet
                bytesSent += DATA_SIZE;
                seqNum++;
            } else {
                printf("Received out of order or incorrect ACK, expected: %u, got: %u\n", seqNum, ack.seq_num);
                fseek(file, -DATA_SIZE, SEEK_CUR); // Move file pointer back for retransmission
            }
        } else {
            break; // No more data to read/send
        }
    }

    fclose(file);
    close(sockfd);
}


int main(int argc, char** argv) {
    // This is a skeleton of a main function.
    // You should implement this function more completely
    // so that one can invoke the file transfer from the
    // command line.
    int hostUDPport;
    unsigned long long int bytesToTransfer;
    char* hostname = NULL;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    hostUDPport = (unsigned short int) atoi(argv[2]);
    hostname = argv[1];
    bytesToTransfer = atoll(argv[4]);

    return (EXIT_SUCCESS);
}