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
    int data_len;
    char data[DATA_SIZE];
    int acked;
};

struct ack_packet {
    int seq_num;
};

int last_received_seq = -1;
int RWND_idx = 0;
struct packet RWND[100]; //size is a placeholder 
int totalBytesReceived = 0;
int totalToReceive = 1;
int compare(const void *a, const void *b) {
    return ((struct packet*)a)->seq_num - ((struct packet*)b)->seq_num;
}

void sortArr(struct packet arr[]){
    qsort(arr, RWND_idx, sizeof(struct packet), compare);
}

// Function to receive a packet
int receive_packet(int sockfd, struct packet* packet, struct sockaddr_in* sender_addr, ssize_t *bytesReceived) {
    char buffer[sizeof(*packet)]; // Correctly use sizeof(*packet) to get the size of the structure
    socklen_t addr_len = sizeof(*sender_addr);
     *bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)sender_addr, &addr_len);
    if (*bytesReceived < 0){
        perror("recvfrom failed in receiver packets");
        return 0;
    }
    
    printf("Received %zd bytes\n", *bytesReceived); // Print the number of received bytes
    // Optionally print the sender's IP address
    char senderIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,(const void *)&sender_addr->sin_addr, senderIP, sizeof(senderIP));
    printf("From %s\n", senderIP);

    // If the data is expected to be text, print the received text (ensure it's null-terminated)
    buffer[*bytesReceived - 4] = '\0'; // Make sure there's no buffer overflow here
    memcpy(packet, buffer, sizeof(*packet));
    return 1; // Success
}

// Function to send packet
int send_packet(struct packet packettosend, int sockfd, struct sockaddr_in receiver_addr, size_t buffer_size){
    char buffer[buffer_size];
    memcpy(buffer, &packettosend, sizeof(packettosend));
    if (sendto(sockfd, buffer, buffer_size, 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
            return 0;
        }
    return 1;
};


int send_ack(int sockfd, struct sockaddr_in receiver_addr , int seq){
    struct ack_packet ack;
    ack.seq_num = seq;
    printf("sending acknowledgment: %d\n", seq );
    char buffer[sizeof(struct ack_packet)];
    memcpy(buffer, &ack, sizeof(ack));
    if (sendto(sockfd, buffer, sizeof(struct ack_packet), 0, (const struct sockaddr *) &receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("failed to send ack ");
        return 0;
    }
    return 1;
}

/*
Helper function to write packet data to file
Makes sure that we only write at the specified writeRate
*/
int write_packet_to_file(struct packet packet, int writeRate){
    char buffer[DATA_SIZE];
    char currBuffer[writeRate];
    memcpy(buffer,&packet.data,sizeof(packet.data));
             printf("Received packet contains: \"%s\"\n", buffer);
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

int initiate_connection(int sockfd, int writeRate, struct sockaddr_in *sender_addr){
   // struct packet SYN;

    // set timeout
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0; // value to timeout rn
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting options");
    }

    printf("Received handshake initiation.\n");

    struct packet SYN_ACK;
    SYN_ACK.seq_num = -1;
    sprintf(SYN_ACK.data,"%d",writeRate);
    SYN_ACK.acked = 0;
    
    while(1){
        // check size (last argument)
        if(send_packet(SYN_ACK, sockfd, *sender_addr, 500) == 0){
            perror("failure to send write rate");
        } else{
            printf("sent write rate succesfully\n");
        }

        char buffer[sizeof(struct ack_packet)];
        socklen_t addr_len = sizeof(*sender_addr);
        ssize_t bytesReceived = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)sender_addr, &addr_len);

        //// TIMEOUT CHECK /////////////////
        if (bytesReceived >= 0) {
            struct ack_packet received;
            memcpy(&received,buffer,sizeof(buffer));
            if(received.seq_num == -1){
                printf("received ack\n");
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


void rrecv(unsigned short int myUDPport, char* destinationFile, unsigned long long int writeRate) {
    int sockfd;
    struct sockaddr_in my_addr;
    char buffer[508];   

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
        printf("receiving.....\n");
        if(receive_packet(sockfd,&curr_packet,&sender_addr,&bytesReceived) == 0) continue;
        
        // handshake check
        if(curr_packet.seq_num == - 2){
            break;
        }
        if(curr_packet.seq_num == - 1){
            printf("handshake....\n");
            totalToReceive = atoi(curr_packet.data);
            printf("RECEIVING : %d\n", totalToReceive);
            initiate_connection(sockfd,  writeRate, &sender_addr);
        }
        else{
            // acknowledge packet

            // timoeut test
            

            if(!send_ack(sockfd,sender_addr,curr_packet.seq_num)){
                printf("failed to send ack\n");
            }
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
                printf("writing in order: %d\n",curr_packet.seq_num);
                write_packet_to_file(curr_packet, writeRate);
            } else {
                printf("write : %d\n", curr_packet.seq_num);
                write_packet_to_file(curr_packet, writeRate);
               // last_received_seq++;
                printf("Last received = %d\n RWND_IDX = %d\n",last_received_seq, RWND_idx);
               
                for(int i = 0; i < RWND_idx; i++){
                    // write the stuff in the window in order
                    if(RWND[i].seq_num == last_received_seq + 1){
                        printf("writing out of order: %d\n",curr_packet.seq_num);
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

    printf("EXITING\n");
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
    unsigned long long int writeRate = 10; // Placeholder, adjust as needed

    rrecv(myUDPport, destinationFile, writeRate);

    return EXIT_SUCCESS;
}
