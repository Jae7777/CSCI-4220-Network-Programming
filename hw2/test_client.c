#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    
    char buffer[1024];
    int bytes;
    
    // Read welcome message
    bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Server: %s", buffer);
    }
    
    // Send username
    send(sock, "testuser\n", 9, 0);
    
    // Read response
    bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Server: %s", buffer);
    }
    
    // Send a message
    send(sock, "Hello everyone!\n", 16, 0);
    
    // Read any responses
    sleep(1);
    bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Server: %s", buffer);
    }
    
    // Send quit command
    send(sock, "/quit\n", 6, 0);
    
    close(sock);
    return 0;
}
