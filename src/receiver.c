#include <stdbool.h>
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


#include <signal.h>

FILE *file; // Make sure file is accessible in the signal handler context
#define DATA_SIZE  508

///////////////////////////////// TESTING FUNCTIONS ////////////////////////////////////////////
void handle_timeout() {
    printf("Timeout occurred.\n");
    // Handle the timeout, such as retrying the operation, logging, or cleaning up resources.
}

void sigint_handler(int sig) {
    printf("Caught signal %d, closing file and exiting.\n", sig);
    if (file != NULL) {
        fclose(file); // Close the file on SIGINT
        file = NULL; // Prevent use-after-free
    }
    exit(0); // Exit gracefully
}


/////////////////////////////// REAL TING //////////////////////////////////////////

struct packet {
    int seq_num;
    char data[DATA_SIZE];
    int acked;
};

// Function to send packet
int send_packet(struct packet packettosend, int sockfd, struct sockaddr_in receiver_addr, size_t buffer_size){
    char buffer[buffer_size];
    memcpy(buffer, &packettosend, sizeof(packettosend));
    if (sendto(sockfd, buffer, buffer_size, 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
            return 0;
        }
    return 1;
};

// Function to receive a packet
int receive_packet(int sockfd, struct packet* packet, struct sockaddr_in* sender_addr) {
    char buffer[sizeof(*packet)]; // Correctly use sizeof(*packet) to get the size of the structure
    socklen_t addr_len = sizeof(*sender_addr);
    
    ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)sender_addr, &addr_len);
    if (bytesReceived < 0) {
        perror("recvfrom failed");
        return 0;
    }
  
    memcpy(packet, buffer, sizeof(*packet));
    return 1; // Success
}


int initiate_connection(int sockfd, int writeRate){
    struct sockaddr_in sender_addr;
    struct packet SYN;
    if (receive_packet(sockfd, &SYN, &sender_addr) == 0 && SYN.seq_num == -1) {
        printf("Received handshake initiation.\n");

        struct packet SYN_ACK;
        SYN_ACK.seq_num = -1;
        
        
        memcpy(buffer, &wri)
    }


}
void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    int sockfd;
    struct sockaddr_in my_addr;
    char buffer[1024];
    FILE *file;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(myUDPport);

    if (bind(sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    file = fopen(destinationFile, "wb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr); // Correct type for len
    int n;

     // set timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000000; // value to timeout rn
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting options");
    }

    ////// TIMEOUT CHECK /////////////////
    // if((n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &sender_addr, &addr_len)) < 0){
    //     if (n < 0) {
    //         if (errno == EAGAIN || errno == EWOULDBLOCK) {
    //             // Timeout detected
    //             handle_timeout();
    //         }
    //     }
    // }

    while ((n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &sender_addr, &addr_len)) > 0) {
        printf("Received %d bytes\n", n); // Print the number of received bytes
        // Optionally print the sender's IP address
        char senderIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, senderIP, sizeof(senderIP));
        printf("From %s\n", senderIP);

        // If the data is expected to be text, print the received text (ensure it's null-terminated)
        buffer[n] = '\0'; // Make sure there's no buffer overflow here
        printf("Received packet contains: \"%s\"\n", buffer);

        if (fwrite(buffer, 1, n, file) != n) {
            perror("Failed to write to file");
            break; // Handle write error
        }
        fflush(file); 
    }


    fclose(file);
    close(sockfd);
}


int main(int argc, char** argv) {
    signal(SIGINT, sigint_handler);
    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned short int myUDPport = (unsigned short int)atoi(argv[1]);
    char* destinationFile = argv[2];
    unsigned long long int writeRate = 0; // Placeholder, adjust as needed

    rrecv(myUDPport, destinationFile, writeRate);

    return EXIT_SUCCESS;
}
