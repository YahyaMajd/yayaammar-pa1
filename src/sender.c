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

#define BUFFER_SIZE 1024  // Adjust based on your MTU and header size
#define HEADER_SIZE 8     // Example header size for sequence number (and possibly other info)
#define DATA_SIZE (BUFFER_SIZE - HEADER_SIZE)
#define ACK_TIMEOUT 2     // Timeout in seconds for ACKs


/**
 * Function: rsend
 * ----------------
 * Sends the first 'bytesToTransfer' bytes of a file specified by 'filename'
 * to a receiver at 'hostname':'hostUDPport' using UDP, implementing a basic
 * form of reliability over an otherwise unreliable protocol.
 *
 * The function splits the file into chunks, each preceded by a header containing
 * a sequence number. It then sends these chunks over UDP. The receiver is expected
 * to send back acknowledgments (ACKs) for each received chunk, although this
 * implementation does not yet handle the ACK reception and retransmission of lost packets.
 *
 * Parameters:
 * - hostname: The domain name or IP address of the receiver.
 * - hostUDPport: The UDP port number on the receiver to send data to.
 * - filename: The path to the file to be transferred.
 * - bytesToTransfer: The number of bytes from the file to be transferred.
 *
 * Implementation Details:
 * 1. Initialization: Creates a UDP socket and sets a timeout for ACK reception.
 * 2. Receiver Address Configuration: Sets up the receiver's address using the provided hostname and port.
 * 3. File Reading: Opens the specified file and reads it in chunks.
 * 4. Data Transmission: Each chunk, prefixed with a sequence number, is sent to the receiver.
 * 5. Sequence Numbering: Implements a simple sequence number mechanism for tracking packets.
 * 6. ACK Handling (TODO): A placeholder exists for implementing ACK reception and packet retransmission logic.
 * 7. Cleanup: Closes the file and socket resources before exiting.
 *
 * Note: The actual reliability mechanism (ACK handling and retransmission) is not implemented and
 * must be added to achieve reliable data transfer over UDP.
 *
 * Usage:
 * The function is designed to be called with command line parameters specifying the receiver details,
 * the file to be transferred, and the number of bytes to transfer.
 */


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

    // Read and send file in chunks
    while (bytesSent < bytesToTransfer && fread(buffer + HEADER_SIZE, 1, DATA_SIZE, file) > 0) {
        // Prepare packet with sequence number (and possibly other header info)
        *((unsigned int*)buffer) = htonl(seqNum);

        // Calculate and send packet size
        ssize_t packetSize = DATA_SIZE + HEADER_SIZE;
        if (sendto(sockfd, buffer, packetSize, 0, (struct sockaddr*)&receiverAddr, sizeof(receiverAddr)) < 0) {
            perror("Sendto failed");
            break;
        }

        // Increment counters
        bytesSent += DATA_SIZE;
        seqNum++;

        // TODO: Implement ACK receiving and retransmission logic
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

    // Now actually use them
    rsend(hostname, hostUDPport, argv[3], bytesToTransfer);

    return (EXIT_SUCCESS);
}