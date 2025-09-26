#include "unp.h"
#include <arpa/inet.h>
#include <string.h>

#define TFTP_RRQ   1
#define TFTP_WRQ   2
#define TFTP_DATA  3
#define TFTP_ACK   4
#define TFTP_ERROR 5
#define DATA_SIZE 512
#define PACKET_BUFFER_SIZE (DATA_SIZE + 4)

// TFTP Error Codes
#define TFTP_ERR_NOT_DEFINED 0
#define TFTP_ERR_FILE_NOT_FOUND 1
#define TFTP_ERR_ACCESS_VIOLATION 2
#define TFTP_ERR_DISK_FULL 3
#define TFTP_ERR_ILLEGAL_OP 4
#define TFTP_ERR_UNKNOWN_TID 5
#define TFTP_ERR_FILE_EXISTS 6
#define TFTP_ERR_NO_SUCH_USER 7

static void sig_alrm(int signo);
volatile sig_atomic_t timeout_occured = 0;

#define TFTP_PORT 69 // Standard TFTP port, for reference, not for use in binding.

// We will use one global variable to track the next available port.
// This is safe because of fork(). The parent updates it, and each child
// gets a copy-on-write page. When the child increments it, it doesn't
// affect the parent's version. However, for this to work correctly,
// the parent needs to manage the port number.
// A better approach is needed. Let's pass it as an argument.

void handle_request(int starting_port, int end_port, SA *pcliaddr, socklen_t clilen, char *mesg);
void dg_tftp_listen(int sockfd, int start_port, int end_port, SA *pcliaddr, socklen_t clilen);
void handle_rrq(int sockfd, char *request);
void handle_wrq(int sockfd, char *request);
void send_error(int sockfd, int error_code, const char *error_msg);

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if (argc != 3) {
        err_quit("usage: tftp.out <start_port> <end_port>");
    }

    int start_port = atoi(argv[1]);
    int end_port = atoi(argv[2]); // We'll use this later

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(start_port);

    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    Bind(sockfd, (SA *)&servaddr, sizeof(servaddr));

    dg_tftp_listen(sockfd, start_port, end_port, (SA *)&cliaddr, sizeof(cliaddr));

    exit(0);
}

void dg_tftp_listen(int sockfd, int start_port, int end_port, SA *pcliaddr, socklen_t clilen) {
    socklen_t len;
    char mesg[MAXLINE];
    pid_t childpid;
    
    // We start using ports *after* the listening port.
    int next_port = start_port + 1;

    for (;;) {
        len = clilen;
        printf("Waiting for request...\n");
        // We need to pass the received message to the child.
        // Let's receive it here and pass it.
        ssize_t n = Recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);
        mesg[n] = '\0'; // null terminate

        if ((childpid = Fork()) == 0) { // Child process
            handle_request(next_port, end_port, pcliaddr, len, mesg);
            exit(0); // Child terminates after handling request
        } else { // Parent process
            // The parent increments the port for the *next* child.
            next_port++;
            if (next_port > end_port) {
                // Handle port exhaustion if necessary, for now, just print.
                fprintf(stderr, "Warning: Port range exhausted.\n");
                next_port = start_port + 1; // Or some other strategy
            }
        }
    }
}

void handle_request(int port_to_use, int end_port, SA *pcliaddr, socklen_t clilen, char *mesg) {
    if (port_to_use > end_port) {
        // TODO: Send a proper TFTP error back
        err_msg("No available ports to handle the request.");
        return;
    }

    printf("Child process created to handle request from %s, using port %d\n", Sock_ntop(pcliaddr, clilen), port_to_use);

    // Create a new socket for the transfer
    int data_sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_to_use);

    Bind(data_sockfd, (SA *)&servaddr, sizeof(servaddr));

    // Parse opcode from the message
    uint16_t opcode = ntohs(*(uint16_t*)mesg);

    // The client's address is in pcliaddr. We need to connect our new socket to it.
    Connect(data_sockfd, pcliaddr, clilen);

    switch (opcode) {
        case TFTP_RRQ:
            printf("Received RRQ\n");
            handle_rrq(data_sockfd, mesg);
            break;
        case TFTP_WRQ:
            printf("Received WRQ\n");
            handle_wrq(data_sockfd, mesg);
            break;
        default:
            send_error(data_sockfd, TFTP_ERR_ILLEGAL_OP, "Invalid TFTP operation.");
            fprintf(stderr, "Invalid opcode: %d\n", opcode);
            break;
    }

    Close(data_sockfd);
}

void handle_wrq(int sockfd, char *request) {
    char filename[128];
    char mode[32];

    sscanf(request + 2, "%s", filename);
    sscanf(request + 2 + strlen(filename) + 1, "%s", mode);

    printf("WRQ for filename: '%s', mode: '%s'\n", filename, mode);

    if (strcasecmp(mode, "octet") != 0) {
        send_error(sockfd, TFTP_ERR_ILLEGAL_OP, "Only octet mode is supported.");
        return;
    }

    // Check if file already exists
    if (access(filename, F_OK) == 0) {
        send_error(sockfd, TFTP_ERR_FILE_EXISTS, "File already exists.");
        return;
    }

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        send_error(sockfd, TFTP_ERR_ACCESS_VIOLATION, "Cannot create file.");
        return;
    }

    char recv_buffer[PACKET_BUFFER_SIZE];
    char send_buffer[4]; // For ACKs
    uint16_t block_number = 0;

    // Send initial ACK(0)
    uint16_t temp_opcode = htons(TFTP_ACK);
    memcpy(send_buffer, &temp_opcode, sizeof(temp_opcode));
    uint16_t temp_block = htons(block_number);
    memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));
    send(sockfd, send_buffer, 4, 0);
    printf("Sent ACK for block 0\n");

    block_number++;

    // Setup SIGALRM handler
    struct sigaction sa;
    sa.sa_handler = sig_alrm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    while (1) {
        int retransmit_count = 0;
        ssize_t n = -1;
        
        while (retransmit_count < 10) {
            timeout_occured = 0;
            alarm(1);
            n = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
            alarm(0);

            if (n >= 0) {
                break;
            }
            if (errno == EINTR) {
                if (timeout_occured) {
                    printf("Timeout waiting for DATA block %d. Retransmitting ACK for block %d...\n", block_number, block_number - 1);
                    // Retransmit previous ACK
                    temp_block = htons(block_number - 1);
                    memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));
                    send(sockfd, send_buffer, 4, 0);
                    retransmit_count++;
                    continue;
                }
            } else {
                err_sys("recv error");
                fclose(file);
                return;
            }
        }

        if (retransmit_count == 10) {
            fprintf(stderr, "Connection timed out waiting for DATA block %d.\n", block_number);
            fclose(file);
            remove(filename); // Clean up partial file
            return;
        }

        uint16_t opcode, recv_block;
        memcpy(&opcode, recv_buffer, sizeof(opcode));
        opcode = ntohs(opcode);
        memcpy(&recv_block, recv_buffer + 2, sizeof(recv_block));
        recv_block = ntohs(recv_block);


        if (opcode != TFTP_DATA) {
            send_error(sockfd, TFTP_ERR_ILLEGAL_OP, "Expected DATA packet.");
            break;
        }

        if (recv_block == block_number) {
            fwrite(recv_buffer + 4, 1, n - 4, file);
            
            // Send ACK for current block
            temp_opcode = htons(TFTP_ACK);
            memcpy(send_buffer, &temp_opcode, sizeof(temp_opcode));
            temp_block = htons(block_number);
            memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));
            send(sockfd, send_buffer, 4, 0);
            printf("Sent ACK for block %d\n", block_number);

            if (n - 4 < DATA_SIZE) {
                printf("File transfer completed.\n");
                break; // Last packet
            }
            block_number++;
        } else if (recv_block < block_number) {
            // Re-send ACK for this old packet, in case our ACK was lost
            printf("Received duplicate block %d. Resending ACK.\n", recv_block);
            temp_opcode = htons(TFTP_ACK);
            memcpy(send_buffer, &temp_opcode, sizeof(temp_opcode));
            temp_block = htons(recv_block);
            memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));
            send(sockfd, send_buffer, 4, 0);
        } else {
            // Block from the future?
            send_error(sockfd, TFTP_ERR_ILLEGAL_OP, "Unexpected block number.");
            break;
        }
    }

    fclose(file);
}

void handle_rrq(int sockfd, char *request) {
    char filename[128];
    char mode[32];
    
    // The request format is: | 2 bytes opcode | filename | 1 byte 0 | mode | 1 byte 0 |
    sscanf(request + 2, "%s", filename);
    sscanf(request + 2 + strlen(filename) + 1, "%s", mode);

    printf("RRQ for filename: '%s', mode: '%s'\n", filename, mode);

    if (strcasecmp(mode, "octet") != 0) {
        send_error(sockfd, TFTP_ERR_NOT_DEFINED, "Only octet mode is supported.");
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        send_error(sockfd, TFTP_ERR_FILE_NOT_FOUND, "File not found.");
        return;
    }

    uint16_t block_number = 1;
    char send_buffer[PACKET_BUFFER_SIZE];
    char recv_buffer[PACKET_BUFFER_SIZE];
    ssize_t bytes_read;

    // Setup SIGALRM handler
    struct sigaction sa;
    sa.sa_handler = sig_alrm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    while ((bytes_read = fread(send_buffer + 4, 1, DATA_SIZE, file)) > 0) {
        // Construct DATA packet
        uint16_t temp_opcode = htons(TFTP_DATA);
        memcpy(send_buffer, &temp_opcode, sizeof(temp_opcode));
        uint16_t temp_block = htons(block_number);
        memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));
        
        int retransmit_count = 0;
        while (retransmit_count < 10) {
            timeout_occured = 0;
            send(sockfd, send_buffer, bytes_read + 4, 0);
            alarm(1); // Set 1-second alarm

            ssize_t n = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);

            if (n >= 0) {
                alarm(0); // Cancel alarm
                uint16_t ack_opcode, ack_block;
                memcpy(&ack_opcode, recv_buffer, sizeof(ack_opcode));
                ack_opcode = ntohs(ack_opcode);
                memcpy(&ack_block, recv_buffer + 2, sizeof(ack_block));
                ack_block = ntohs(ack_block);


                if (ack_opcode == TFTP_ACK && ack_block == block_number) {
                    printf("Received ACK for block %d\n", block_number);
                    break; // Correct ACK received, move to next block
                } else {
                    // Wrong ACK or packet, ignore and wait for correct one or timeout
                    // This handles the "Sorcerer's Apprentice Syndrome"
                    continue;
                }
            } else if (errno == EINTR) {
                if (timeout_occured) {
                    printf("Timeout waiting for ACK for block %d. Retransmitting... (Attempt %d)\n", block_number, retransmit_count + 1);
                    retransmit_count++;
                    continue;
                }
            } else {
                err_sys("recv error");
                break;
            }
        }

        if (retransmit_count == 10) {
            fprintf(stderr, "Connection timed out after 10 retransmissions for block %d.\n", block_number);
            fclose(file);
            return;
        }

        block_number++;

        if (bytes_read < DATA_SIZE) {
            break; // Last packet
        }
    }
    
    // Check if the loop terminated because of an empty file or a read that is a multiple of 512
    if ((bytes_read == 0 && ftell(file) == 0) || (bytes_read == DATA_SIZE)) {
         // File is empty, or file size is a multiple of 512.
         // Send one final DATA packet with 0 bytes.
        if (bytes_read == DATA_SIZE) {
            block_number++;
        }
        
        uint16_t temp_opcode = htons(TFTP_DATA);
        memcpy(send_buffer, &temp_opcode, sizeof(temp_opcode));
        uint16_t temp_block = htons(block_number);
        memcpy(send_buffer + 2, &temp_block, sizeof(temp_block));

        int retransmit_count = 0;
        while (retransmit_count < 10) {
            timeout_occured = 0;
            send(sockfd, send_buffer, 4, 0);
            alarm(1);

            ssize_t n = recv(sockfd, recv_buffer, sizeof(recv_buffer), 0);
            if (n >= 0) {
                alarm(0);
                uint16_t ack_opcode, ack_block;
                memcpy(&ack_opcode, recv_buffer, sizeof(ack_opcode));
                ack_opcode = ntohs(ack_opcode);
                memcpy(&ack_block, recv_buffer + 2, sizeof(ack_block));
                ack_block = ntohs(ack_block);
                if (ack_opcode == TFTP_ACK && ack_block == block_number) {
                    printf("Received final ACK for block %d\n", block_number);
                    break;
                }
            } else if (errno == EINTR) {
                if (timeout_occured) {
                    retransmit_count++;
                    continue;
                }
            } else {
                err_sys("recv error");
                break;
            }
        }
        if (retransmit_count == 10) {
             fprintf(stderr, "Connection timed out waiting for final ACK.\n");
        }
    }


    printf("File transfer completed.\n");
    fclose(file);
}

void send_error(int sockfd, int error_code, const char *error_msg) {
    char buffer[PACKET_BUFFER_SIZE];
    size_t msg_len = strlen(error_msg);
    
    uint16_t temp_opcode = htons(TFTP_ERROR);
    memcpy(buffer, &temp_opcode, sizeof(temp_opcode));
    uint16_t temp_code = htons(error_code);
    memcpy(buffer + 2, &temp_code, sizeof(temp_code));
    strcpy(buffer + 4, error_msg);
    buffer[4 + msg_len] = 0;

    send(sockfd, buffer, 4 + msg_len + 1, 0);
    printf("Sent ERROR packet: %s\n", error_msg);
}

static void sig_alrm(int signo) {
    timeout_occured = 1;
    return;
}
