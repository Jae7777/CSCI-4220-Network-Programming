#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_RESPONSE 8192

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 9000\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char* server_ip = argv[1];
    int port = atoi(argv[2]);
    
    // Create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }
    
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server %s:%d\n", server_ip, port);
    
    // Read initial prompt
    char buffer[MAX_RESPONSE];
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
    
    // Read N from user
    int N;
    printf("Enter N: ");
    if (scanf("%d", &N) != 1) {
        fprintf(stderr, "Invalid input\n");
        exit(EXIT_FAILURE);
    }
    
    // Send N to server
    char msg[32];
    snprintf(msg, sizeof(msg), "%d\n", N);
    write(sockfd, msg, strlen(msg));
    
    // Read and display response
    printf("\nServer response:\n");
    printf("================\n");
    
    while ((n = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }
    
    printf("================\n");
    
    close(sockfd);
    return 0;
}

