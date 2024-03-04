/*
@file receiver.c
@brief the receiver file, implements rrcv 
@author Ammar Sallam (asallam02)
@author Yahya Abulmagd (YahyaMajd)

@bugs no known bugs
*/

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

FILE *file;
#define DATA_SIZE  508
#define WRITERATE  508
#define BUFFER_SIZE 520
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

int last_received_seq = -1;
int RWND_idx = 0;
struct packet RWND[100];
int totalBytesReceived = 0;
int totalToReceive = 1;




int compare(const void *a, const void *b) {
    return ((struct packet*)a)->seq_num - ((struct packet*)b)->seq_num;
}

void sortArr(struct packet arr[]){
    qsort(arr, RWND_idx, sizeof(struct packet), compare);
}

/*
@brief helper function to receive packets, deserializes the data coming on into the packet data structure

@param sockfd: socket information
@param packet: a pointer to store the packet to be received
@param sender_addr: the sender address to receive data from
@param bytesReceived: a pointer to store the amount of bytes that have been received

@return 0 in case of failure, 1 in case of success
*/
int receive_packet(int sockfd, struct packet* packet, struct sockaddr_in* sender_addr, ssize_t *bytesReceived) {
    char buffer[sizeof(*packet)]; // Correctly use sizeof(*packet) to get the size of the structure
    socklen_t addr_len = sizeof(*sender_addr);
     *bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)sender_addr, &addr_len);
    if (*bytesReceived < 0){
        perror("recvfrom failed in receiver packets");
        return 0;
    }

    // If the data is expected to be text, print the received text (ensure it's null-terminated)
    buffer[*bytesReceived] = '\0'; // Make sure there's no buffer overflow here
    memcpy(packet, buffer, sizeof(*packet));
    return 1; // Success
}

/*
@brief helper function to send packets, serializes packets to bytes to be sent

@param packettosend: the packet to be sent
@param sockfd: socket information
@param receiver_addr: the receiver address to send data to
@param buffer_size: the size of the data to be sent

@return 0 in case of failure, 1 in case of success
*/
int send_packet(struct packet packettosend, int sockfd, struct sockaddr_in receiver_addr, size_t buffer_size){
    char buffer[buffer_size];
    memcpy(buffer, &packettosend, sizeof(packettosend));
    receiver_addr.sin_family = AF_INET;
    if (sendto(sockfd, buffer, buffer_size, 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
            perror("failed to send\n");
            return 0;
        }
    return 1;
};

/*
@brief helper function to send acks

@param sockfd: socket information
@param receiver_addr: the receiver address to send data to
@param seq: the sequence number of the ack

@return 0 in case of failure, 1 in case of success
*/
int send_ack(int sockfd, struct sockaddr_in receiver_addr , int seq){
    struct ack_packet ack;
    ack.seq_num = seq;
    char buffer[sizeof(struct ack_packet)];
    memcpy(buffer, &ack, sizeof(ack));
    if (sendto(sockfd, buffer, sizeof(struct ack_packet), 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("failed to send ack ");
        return 0;
    }
    return 1;
}


/*
@brief Helper function to write packet data to file

Writes data to file at a specified writeRate, 
also modifies totalBytesReceived based on the amount of bytes written to file

@param packet: the packet to be written to file
@param writeRate: the maximum write rate per second 

@return 0 in case of failure, 1 in case of success
*/
int write_packet_to_file(struct packet packet, int writeRate){
    char buffer[DATA_SIZE];
    char currBuffer[writeRate];
    memcpy(buffer,&packet.data,sizeof(packet.data));
    int bytesWritten = 0;
    while(bytesWritten < packet.data_len){

        if(writeRate == 0){
            if (fwrite(buffer, 1, packet.data_len, file) != (packet.data_len)) {
                perror("Failed to write to file");
                return 0;
            }
            bytesWritten = packet.data_len;
            fflush(file);
        } else {
            for(int i = 0 && i + bytesWritten < packet.data_len; i < writeRate; i++){
                currBuffer[i] = buffer[bytesWritten + i];
            }
            time_t start_time = time(NULL); // curr time

            int write_amt = writeRate;
            if(bytesWritten + writeRate > packet.data_len){
                write_amt = packet.data_len - bytesWritten;
            } 
            if (fwrite(currBuffer, 1, write_amt, file) != (write_amt)) {
                perror("Failed to write to file");
                return 0;
            }
            bytesWritten += write_amt;
            fflush(file);
            time_t end_time = time(NULL);
            sleep(difftime(end_time, start_time));
        }
    }
    totalBytesReceived += bytesWritten;
    return 1;
}

/*
@brief helper function to initiate connection with the sender

This function initiates the connection with the sender upon receiving 
a packet with sequence number = -1, it send the write rate to the 
sender and awaits an ack. After which the receiver will begin 
receiving normally

@param sockfd: socket information
@param writeRate: the write rate to communicate to the sender
@param sender_addr: the receiver address to send data to

@return 0 in case of failure, 1 in case of success
*/
int initiate_connection(int sockfd, int writeRate, struct sockaddr_in *sender_addr){

    // set timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0; 
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting options");
    }

    struct packet SYN_ACK;
    SYN_ACK.seq_num = -1;
    sprintf(SYN_ACK.data,"%d",writeRate);
    SYN_ACK.acked = 0;
    
    while(1){
        // check size (last argument)
        if(send_packet(SYN_ACK, sockfd, *sender_addr, BUFFER_SIZE) == 0){
            perror("failure to send write rate");
        } 
        char buffer[sizeof(struct ack_packet)];
        socklen_t addr_len = sizeof(*sender_addr);
        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)sender_addr, &addr_len);

        // check for timeout
        if (bytesReceived >= 0) {
            struct ack_packet received;
            memcpy(&received,buffer,sizeof(buffer));
            if(received.seq_num == -1){
                return 1;
            }     
        }
        else{
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout detected
                perror("timeout");
            } else{
                perror("recvfrom failed");
                return 0;
            }
        }
    }

    return 0; 
}

/*
@brief the main function for reliably receiving data

This is the main function of the receiver, it initiates connection
with the sender, keeps receiving messages, writes data to file in
order, and then terminates then closes when the sender is done 
sending. 

@param myUDPport: port number for the receiver to receive on
@param destination file: the file to write the incoming data to
@param writeRate: the maximum bytes/s to be written to the file
*/
void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    int sockfd;
    struct sockaddr_in my_addr;   

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
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
    ssize_t bytesReceived;
    while (totalBytesReceived < totalToReceive) {
        struct packet curr_packet;
        if(receive_packet(sockfd,&curr_packet,&sender_addr,&bytesReceived) == 0) continue;
        
        // handshake check
        if(curr_packet.seq_num == - 2){
            break;
        }
        if(curr_packet.seq_num == - 1){
            totalToReceive = atoi(curr_packet.data);
            initiate_connection(sockfd,  writeRate, &sender_addr);
        }
        else{
            // acknowledge packet
            send_ack(sockfd,sender_addr,curr_packet.seq_num);
            // if packet already received send ack ^^ and continue to next packet
            if(curr_packet.seq_num <= last_received_seq){
                continue;
            }
            // make sure we're getting packets in order
            if(curr_packet.seq_num != last_received_seq + 1){
                RWND[RWND_idx] = curr_packet;
                RWND_idx++;
                sortArr(RWND);
                continue;
            } else {
                last_received_seq++;
            }
            
            if(RWND_idx == 0){
                write_packet_to_file(curr_packet, writeRate);
            } else {
                write_packet_to_file(curr_packet, writeRate);
               
                for(int i = 0; i < RWND_idx; i++){
                    // write the stuff in the window in order
                    if(RWND[i].seq_num == last_received_seq + 1){
                        write_packet_to_file(RWND[i], writeRate);
                        last_received_seq++;
                    } else{
                        // make sure you bring them back so array starts from 0
                        int last_index = i;
                        for(int j = i; j < RWND_idx; j++){
                            RWND[j - i] = RWND[j];
                            last_index = j - i;
                        }
                        RWND_idx = last_index;
                    }
                }
            }
        }        
    }

    fclose(file);
    close(sockfd);
}


int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unsigned short int myUDPport = (unsigned short int)atoi(argv[1]);
    char* destinationFile = argv[2];
    unsigned long long int writeRate = WRITERATE; 

    rrecv(myUDPport, destinationFile, writeRate);

    return EXIT_SUCCESS;
}
