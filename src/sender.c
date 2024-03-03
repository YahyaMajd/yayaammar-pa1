
/*
@file sender.c
@brief the sender file, implements rsend 
@author Ammar Sallam (asallam02)
@author Yahya Abulmagd (YahyaMajd)

@bugs no known bugs
*/


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
#define BUFFER_SIZE 520
#define MAX_CWND_SIZE 100 

int packet_size = 0;
int CWND_size = 0;
int num_CWND_occupied = 0;
int pack_num = -1;
int bytesTransferring = 0;

/*
@brief packet structure, used to deserialize incoming packets
*/
struct packet {
    int seq_num;
    int data_len;
    char data[DATA_SIZE];
    int acked;  
};

/*
@brief ack packet structure, used to serialize and deserialize packets
*/
struct ack_packet {
    int seq_num;
};


/*
@brief helper function to send packets, serializes packets to bytes to be sent

@param packettosend: the packet to be sent
@param sockfd: socket information
@param receiver_addr: the receiver address to send data to
@param packet_size: the size of the data to be sent

@return 0 in case of failure, 1 in case of success
*/
int send_packet(struct packet packettosend, int sockfd, struct sockaddr_in receiver_addr, size_t packet_size){
    char buffer[packet_size];
    memcpy(buffer, &packettosend, sizeof(packettosend));
    if (sendto(sockfd, buffer, BUFFER_SIZE, 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
            perror("send packet failed");
            return 0;
        }
    return 1;
};

void print_sender_port(int sockfd) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname failed");
    } else {
        printf("Sender port: %d\n", ntohs(sin.sin_port));
    }
}

/*
@brief helper function to handle timeouts

goes through the congestion window, resends any un-acked
packets

@param CWND: the congestion window array
@param sockfd: socket information
@param receiver_addr: the receiver address to send data to
*/
void handle_timeout(struct packet *CWND[], int sockfd, struct sockaddr_in receiver_addr){
    for(size_t i = 0; i < num_CWND_occupied; i++){
        // check if packet in window is acked, otherwise resend
        if(CWND[i]->acked == 0){
            if(send_packet(*CWND[i], sockfd, receiver_addr, BUFFER_SIZE) == 0){
                perror(" error resending packet");
            }
        }
    }
}

/*
@brief helper function to handle receiving acks

marks packets as acked and slides the congestion 
window if needed

@param CWND: the congestion window array
@param ack_seq_num: the sequence number of the incoming ack

@return 0 in case of failure, 1 in case of success
*/
void handle_ack_recv(struct packet *CWND[], int ack_seq_num){
    for(int i = 0; i < num_CWND_occupied; i++){
        if(CWND[i]->seq_num == ack_seq_num){
            CWND[i]->acked = 1;
            break;
        }
    }

    int num_acked = 0;
    for(int i = 0; i < num_CWND_occupied; i++){
        if(CWND[i]->acked == 1){
            free(CWND[i]);
            num_acked++;
        }
        else break;
    }

    for(int i = num_acked; i < num_CWND_occupied; i++){
        CWND[i - num_acked] = CWND[i];
    }
    num_CWND_occupied -= num_acked;
}

/*
@brief helper function to receive packets, deserializes the data coming on into the packet data structure

@param sockfd: socket information
@param packet: a pointer to store the packet to be received
@param sender_addr: the sender address to receive data from

@return 0 in case of failure, 1 in case of success
*/
int receive_packet(int sockfd, struct packet* packet, struct sockaddr_in* sender_addr) {
    char buffer[sizeof(*packet)]; // Correctly use sizeof(*packet) to get the size of the structure
    socklen_t addr_len = sizeof(*sender_addr);
    
    ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*) &sender_addr, &addr_len);
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

/*
@brief helper function to initiate connection with the receiver

This function initiates the connection with the receiver, it 
sends the expected bytesToTransfer to the receiver. Sets the 
packet size and congestion window size based on receiver write
rate.

@param sockfd: socket information
@param receiver_addr: the receiver address to send/receive data
@param SYN_size: size of the SYN packet

@return 0 in case of failure, 1 in case of success
*/
int initiate_connection(int sockfd, struct sockaddr_in* receiver_addr, size_t SYN_size){
    // send SYN message
    struct packet SYN; 
    SYN.seq_num = pack_num;
    SYN.acked = 0;
    SYN.data_len = SYN_size;
    sprintf(SYN.data,"%d",bytesTransferring);
    // advance global sequence number 
    pack_num++;
    print_sender_port(sockfd);
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
            break;  
        }
    }
    
    // deserialize write rate, figure out the congestion window and packet size
    int write_rate = atoi(write_rate_packet.data);
    // CWND calculation
    packet_size = 520; // 508 data plus three ints
    if(write_rate == 0){
        CWND_size = MAX_CWND_SIZE;
    } else {
        if(write_rate / packet_size < 1) CWND_size = 1;
        else CWND_size = write_rate / packet_size;
    }

    // send ack
    struct ack_packet ack;
    ack.seq_num = write_rate_packet.seq_num;

    char buffer[sizeof(struct ack_packet)];
    memcpy(buffer, &ack, sizeof(ack));
    
    if (sendto(sockfd, buffer, packet_size, 0, (const struct sockaddr *) receiver_addr, sizeof(*receiver_addr)) < 0) {
        perror("failed to send ack");
    }
    return 1;
}

/*
@brief the main function for reliably sending data

This is the main function of the sender, it initiates connection
with the receiver, reads data from file, and sends the data to 
the receiver while handling dropped messages. 

@param hostname: the current host address
@param myUDPport: port number for the receiver to receive on
@param filename: the file to read the data from
@param bytesToTransfer: tthe amount of bytes to send
*/
void rsend(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    bytesTransferring = bytesToTransfer;
    int sockfd;
    struct sockaddr_in receiver_addr;
    char buffer[DATA_SIZE];
    FILE *file;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    // set timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0; 
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

    // establish connection with receiver
    size_t SYN_size = 516; 
    initiate_connection(sockfd, &receiver_addr, SYN_size);

    struct packet *CWND[CWND_size];

    // Read and send the file in chunks
    unsigned long long int bytesSent = 0;
    while (bytesSent < bytesToTransfer && !feof(file)) {
        // if buffer is full or file ended/all data sent, wait for ack/timeout
        if(num_CWND_occupied == CWND_size || bytesSent >= bytesToTransfer){
            // wait for ack/timeout
            char buffer[sizeof(struct ack_packet)];
            socklen_t addr_len = sizeof(receiver_addr);
            ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&receiver_addr, &addr_len);
            if (bytesReceived >= 0) {
                struct ack_packet received;
                memcpy(&received,buffer,sizeof(buffer));

                handle_ack_recv(CWND, received.seq_num);
            }else {
                // check timeout
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout detected
                    handle_timeout(CWND, sockfd, receiver_addr);
                } else{
                    perror("recvfrom failed");
                }
            }

        }
        // otherwise go as normal
        else {
            size_t toRead = sizeof(buffer);
            if (bytesToTransfer - bytesSent < toRead) {
                toRead = bytesToTransfer - bytesSent;
            }
            size_t read = fread(buffer, 1, toRead, file); // need to make sure that we don't reach end of file
            struct packet* send_pkt = malloc(sizeof(struct packet));
            if(send_pkt == NULL){
                perror("malloc failed");
            }
            
            send_pkt->seq_num = pack_num;
            send_pkt->acked = 0;
            pack_num++;
            memcpy(&send_pkt->data,buffer,sizeof(buffer));
            send_pkt->data_len = read;
            if (send_packet(*send_pkt,sockfd,receiver_addr,BUFFER_SIZE) == 0) {
                free(send_pkt);
                break;
            }   

            // update global sequence number and window
            CWND[num_CWND_occupied] = send_pkt;
            num_CWND_occupied++;

            // advance read pointer
            bytesSent += read;
        }
    
    }
    // specify reason to transmission end
    if (bytesSent < bytesToTransfer) {
        struct packet FIN; 
        FIN.seq_num = -2;
        FIN.acked = 0;
        send_packet(FIN, sockfd, receiver_addr, packet_size);
    }
    fclose(file);
    close(sockfd);
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
