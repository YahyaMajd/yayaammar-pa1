#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define HEADER_SIZE 8
#define DATA_SIZE (BUFFER_SIZE - HEADER_SIZE)
#define ACK_TIMEOUT 2  // Timeout in seconds for ACKs
#define INITIAL_PACKETS_SIZE 10  // Initial size of the packets array

struct packet {
    unsigned int seq_num;
    char data[DATA_SIZE];
    int acked;  // 1 if ACK received, 0 otherwise
    time_t send_time;  // Time when the packet was last sent
};

struct ack_packet {
    unsigned int seq_num;
};

struct packet *packets = NULL; // Dynamic array of packets
int num_packets = 0;  // Number of packets currently being tracked
int allocated_packets = 0; // Allocated size of the packets array

void resize_packets_array() {
    if (num_packets >= allocated_packets) {
        int new_size = allocated_packets == 0 ? INITIAL_PACKETS_SIZE : allocated_packets * 2;
        struct packet *new_array = realloc(packets, new_size * sizeof(struct packet));
        if (!new_array) {
            perror("Failed to resize packets array");
            exit(EXIT_FAILURE);
        }
        packets = new_array;
        allocated_packets = new_size;
    }
}

void resend_packets(int sockfd, struct sockaddr_in *receiverAddr) {
    time_t current_time = time(NULL);

    for (int i = 0; i < num_packets; i++) {
        if (!packets[i].acked && difftime(current_time, packets[i].send_time) > ACK_TIMEOUT) {
            char buffer[BUFFER_SIZE];
            *((unsigned int *)buffer) = htonl(packets[i].seq_num);
            memcpy(buffer + HEADER_SIZE, packets[i].data, DATA_SIZE);

            if (sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)receiverAddr, sizeof(*receiverAddr)) < 0) {
                perror("Sendto failed during retransmission");
                continue;
            }

            packets[i].send_time = current_time;
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

    // Initialize dynamic array for packets
    resize_packets_array(); // Ensure we have an initial allocation

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket timeout for receiving ACKs
    tv.tv_sec = ACK_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
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
        ssize_t readBytes = fread(buffer + HEADER_SIZE, 1, DATA_SIZE, file);
        if (readBytes > 0) {
            ssize_t packetSize = HEADER_SIZE + readBytes;
            *((unsigned int*)buffer) = htonl(seqNum); // Sequence number in network byte order

            // Call resend_packets to retransmit any packets that have not been ACKed in time
            resend_packets(sockfd, &receiverAddr);

            // Transmit the packet
            if (sendto(sockfd, buffer, packetSize, 0, (struct sockaddr*)&receiverAddr, sizeof(receiverAddr)) < 0) {
                perror("Sendto failed");
                break;
            }

            // Prepare for tracking the packet
            resize_packets_array();
            packets[num_packets].seq_num = seqNum;
            memcpy(packets[num_packets].data, buffer + HEADER_SIZE, readBytes);
            packets[num_packets].acked = 0; // Initially not acknowledged
            packets[num_packets].send_time = time(NULL); // Record the send time
            num_packets++; // Increment the packet counter

            struct ack_packet ack;
            socklen_t fromlen = sizeof(receiverAddr);

            // Attempt to receive ACK
            if (recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&receiverAddr, &fromlen) >= 0) {
                ack.seq_num = ntohl(ack.seq_num); // Convert to host byte order
                for (int i = 0; i < num_packets; i++) {
                    if (packets[i].seq_num == ack.seq_num) {
                        packets[i].acked = 1; // Mark as acknowledged
                        printf("ACK received for packet %u\n", ack.seq_num);
                        break;
                    }
                }
            } else {
                if (errno != EWOULDBLOCK) {
                    perror("Recvfrom failed");
                    break;
                }
            }

            bytesSent += readBytes;
            seqNum++;
        } else {
            if (ferror(file)) {
                perror("Error reading from file");
                break;
            }
        }
    }

    // Cleanup
    fclose(file);
    close(sockfd);
    free(packets); // Free the dynamically allocated packets array at the end
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