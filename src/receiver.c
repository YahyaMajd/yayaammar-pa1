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
#include <time.h>

#define BUFFER_SIZE 1024  // Adjust based on expected data packet size


/**
 * Function: rrecv
 * ----------------
 * Receives data over UDP and writes it to a specified file, potentially with a rate limit.
 * 
 * This function listens on a specified UDP port for incoming data packets, writes the data
 * to a destination file, and can regulate the data writing speed according to a specified
 * write rate. If the write rate is zero, the function writes data as fast as possible.
 * Otherwise, it limits the writing speed to the specified rate in bytes per second.
 *
 * Parameters:
 * - myUDPport: The UDP port number on which the function listens for incoming data.
 * - destinationFile: Path to the file where received data will be written.
 * - writeRate: Specifies the maximum number of bytes per second to write to the destination file.
 *               A value of 0 means no limit on the write rate.
 *
 * Operation Details:
 * 1. Socket Setup: Initializes a UDP socket and binds it to 'myUDPport' for receiving data.
 * 2. File Opening: Opens 'destinationFile' for writing. Prepares to regulate write speed if needed.
 * 3. Data Reception: Enters a loop to receive data packets. For each packet, the payload is extracted
 *                    and written to the file.
 * 4. Rate Limiting: If 'writeRate' is not zero, calculates the time required to stay within the
 *                   specified rate. This may involve pausing (sleeping) between writes to limit the output.
 * 5. Cleanup: Closes the file and socket once the operation is complete or upon error.
 *
 * Note: This function assumes a continuous stream of data and does not include logic to detect the
 * end of the data stream. Incorporating a protocol for signaling the end of transmission and handling
 * errors and packet loss would be necessary for a robust implementation.
 *
 * Usage:
 * The function is intended to be invoked from the command line, providing a way to specify
 * the UDP port, destination file, and optional write rate.
 */


void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    int sockfd;
    struct sockaddr_in myAddr;
    char buffer[BUFFER_SIZE];
    FILE *file;
    unsigned long long int bytesWritten = 0;
    unsigned long long int startTime, endTime, elapsed, sleepTime;

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

    // Receive data and write to file
    while (1) {
        ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
        if (recvLen < 0) {
            perror("Receive failed");
            break;
        }

        // If writeRate is enforced, capture the start time before writing data
        if (writeRate > 0) {
            startTime = (unsigned long long)time(NULL);
        }

        fwrite(buffer, 1, recvLen, file); // Write received data to file
        bytesWritten += recvLen; // Accumulate total bytes written

        // After writing, if writeRate is enforced, check if rate limiting is needed
        if (writeRate > 0) {
            endTime = (unsigned long long)time(NULL); // Capture end time after write
            elapsed = endTime - startTime; // Calculate elapsed time for the write operation

            // If the write speed exceeds the specified writeRate, calculate and apply sleepTime
            if (bytesWritten / elapsed > writeRate) {
                sleepTime = (bytesWritten / writeRate) - elapsed;
                sleep(sleepTime); // Sleep to throttle write speed to the desired rate
            }
        }
    }

    fclose(file); // Close the file
    close(sockfd); // Close the socket
}


int main(int argc, char** argv) {
    unsigned short int udpPort;
    unsigned long long int writeRate = 0;  // Assume no rate limit by default

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write [write_rate]\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int)atoi(argv[1]);
    if (argc == 4) {
        writeRate = atoll(argv[3]);
    }

    rrecv(udpPort, argv[2], writeRate);

    return 0;
}
