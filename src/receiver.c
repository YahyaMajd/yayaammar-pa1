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

void sigint_handler(int sig) {
    printf("Caught signal %d, closing file and exiting.\n", sig);
    if (file != NULL) {
        fclose(file); // Close the file on SIGINT
        file = NULL; // Prevent use-after-free
    }
    exit(0); // Exit gracefully
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
