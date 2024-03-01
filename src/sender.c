#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define DATA_SIZE 508

int packet_size = 0;
int CWND_size = 0;
int pack_num = -1;
struct packet {
    int seq_num;
    char data[DATA_SIZE];
    int acked;  
    //time_t time_sent; // gonna need to adjust sizes!!!
};

struct ack_packet {
    int seq_num;
};

void handle_timeout(struct packet* CWND[], int seq_num){
    printf("timeout occured");
}

/*
Helper function to send packets
return true if sent successfully and false if errored
*/
// should we create a new buffer everytime or just pass a pointer to a premade buffer?
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

/*
initiates connection with receiver, returns the packet size and congestion window size to be used
*/
int initiate_connection(int sockfd, struct sockaddr_in* receiver_addr, size_t SYN_size){
    // send SYN message
    struct packet SYN; 
    SYN.seq_num = pack_num;
    SYN.acked = 0;
    
    // advance global sequence number 
    pack_num++;

    // send initiation packet
    if(send_packet(SYN, sockfd, *receiver_addr, SYN_size) == 0){
        perror("Failure to send SYN");
    }

    // receive packet with writeRate
    struct packet write_rate_packet;
    while(1){
        if(receive_packet(sockfd, &write_rate_packet, receiver_addr) == 0){
            perror("failure receiving write rate");
        } else {
            printf("received write rate\n");
            break;  
        }
    }
    
    // deserialize write rate, figure out the congestion window and packet size
    
    int write_rate = atoi(write_rate_packet.data);
    printf("%d\n", write_rate);
    // CWND calculation
    packet_size = 516; // 508 data plus two ints
    // CWND_size = packet_size / write_rate;

    // send ack
    struct ack_packet ack;
    ack.seq_num = write_rate_packet.seq_num;

    char buffer[sizeof(struct ack_packet)];
    memcpy(buffer, &ack, sizeof(ack));
    
    if (sendto(sockfd, buffer, packet_size, 0, (const struct sockaddr *) receiver_addr, sizeof(*receiver_addr)) < 0) {
        perror("failed to send ack");
    }
    else{
        printf("sent ack\nexiting initiate connection\n");
    }
    return 1;
}


void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    int sockfd;
    struct sockaddr_in receiver_addr;
    char buffer[508];
    FILE *file;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    // set timeout
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec =0; // value to timeout rn
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting options");
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

    // establish connection with receiver
    size_t SYN_size = 516; 
    initiate_connection(sockfd, &receiver_addr, SYN_size);

   //sleep(15);
    struct packet CWND[CWND_size];

    // Read and send the file in chunks
    unsigned long long int bytesSent = 0;
    while (bytesSent < bytesToTransfer && !feof(file)) {
        size_t toRead = sizeof(buffer);
        if (bytesToTransfer - bytesSent < toRead) {
            toRead = bytesToTransfer - bytesSent;
        }
        size_t read = fread(buffer, 1, toRead, file);
        struct packet send_pkt;
        send_pkt.seq_num  = pack_num;
        printf("packet num : %d\n", pack_num);
        printf("packet itslef num: %d\n", send_pkt.seq_num);
        memcpy(&send_pkt.data,buffer,sizeof(buffer));
        if (send_packet(send_pkt,sockfd,receiver_addr,528) == 0) {
            printf("rsend failed \n");
            break;
        }

        // wait for ack trial 
        char buffer[sizeof(struct ack_packet)];
        socklen_t addr_len = sizeof(receiver_addr);
        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, &addr_len);
        //// TIMEOUT CHECK /////////////////
        if (bytesReceived >= 0) {
            struct ack_packet received;
            memcpy(&received,buffer,sizeof(buffer));
            if(received.seq_num == send_pkt.seq_num){
                printf("received ack for packet %d : \n", received.seq_num);
            } else{
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout detected
                    handle_timeout(CWND,send_pkt.seq_num);
                } else{
                    perror("recvfrom failed");
                }
            }    
        }
        // advance read pointer
        bytesSent += read;
        printf("Sent %zu bytes, total sent: %llu bytes.\n", read, bytesSent);
    }

    // specify reason to transmission end
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
