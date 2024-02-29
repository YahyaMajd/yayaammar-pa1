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

#define CWND 5 // the size of the congestion window
#define ACK_TIMEOUT 30

struct packet {
    unsigned int seq_num;
    char data[DATA_SIZE];
    int acked;  // 1 if ACK received, 0 otherwise
    time_t send_time;  // Time when the packet was last sent
};

struct buffer packet[CWND]; // buffer with size CWND
int smallestunacked = 0; // sequence number of the smallest unacked packet


/* PLAN
Main requirements:
- data is reliably transferred even in the case of dropped or out of order packets
- 70% utilization (cannot just use a send and wait method)

Selective repeat method:
- data from above (from file)
    - if next avilable seq# in window, send packet
- timeout(n): resend packet n, restart timer
- ack(n)
    - mark packet n as received
    - if n is smallest unAcked, advance window to next unAcked

On sender need to handle:
- sending data
- timeouts
- getting acks

On receiver need to handle
- receiving data
- receiving duplicate packets
- receiving out of order data
*/

/*
Handle timeout
*/
void timeout_handler(){
    // resend the packet
}

/*
Send packet
*/
void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket timeout for receiving ACKs
    struct timeval tv;
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

    unsigned long long int bytesSent = 0;
    while(bytesSent < bytesToTransfer){
        // create packet
        // add to buffer
        // send data 
         
        // when buffer is full
        // wait for ack/timeout

        // if first message sent is acked, send new message then redo
        // if timeout, resend first message then redo
            // if this happens should also check if messages further in the buffer have timed out as well
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