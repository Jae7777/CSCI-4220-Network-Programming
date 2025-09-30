#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 5
#define MAX_MSG_LEN 1024
#define BOARD_SIZE 10

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port_offset>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port_offset = atoi(argv[1]);
    int server_port = 9877 + port_offset;

    int listenfd, connfd, sockfd;
    int client_sockets[MAX_CLIENTS];
    int n_clients = 0;
    
    char board[BOARD_SIZE][MAX_MSG_LEN];
    int board_msg_count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }
     for (int i = 0; i < BOARD_SIZE; i++) {
        memset(board[i], 0, MAX_MSG_LEN);
    }

    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    // Forcefully attaching socket to the port
    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, 5) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("TCP server listening on port %d\n", server_port);

    fd_set readfds;
    int max_sd, sd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listenfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        max_sd = listenfd > STDIN_FILENO ? listenfd : STDIN_FILENO;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buf[10];
            if (read(STDIN_FILENO, buf, sizeof(buf)) == 0) {
                printf("Shutting down server due to EOF.\n");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] > 0) {
                        close(client_sockets[i]);
                    }
                }
                close(listenfd);
                exit(EXIT_SUCCESS);
            }
        }

        if (FD_ISSET(listenfd, &readfds)) {
            if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (n_clients >= MAX_CLIENTS) {
                printf("Too many clients, rejecting connection.\n");
                close(connfd);
            } else {
                printf("New client connected.\n");
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) {
                        client_sockets[i] = connfd;
                        n_clients++;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sockfd = client_sockets[i];
            if (FD_ISSET(sockfd, &readfds)) {
                char buffer[MAX_MSG_LEN];
                int n = read(sockfd, buffer, sizeof(buffer) - 1);
                
                if (n == 0) {
                    printf("Client disconnected.\n");
                    close(sockfd);
                    client_sockets[i] = 0;
                    n_clients--;
                } else if (n < 0) {
                    perror("read error");
                    close(sockfd);
                    client_sockets[i] = 0;
                    n_clients--;
                }
                else {
                    buffer[n] = '\0';
                    // Trim newline characters
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strncmp(buffer, "POST ", 5) == 0) {
                        char *msg = buffer + 5;
                        
                        // Add to board (circular)
                        strncpy(board[board_msg_count % BOARD_SIZE], msg, MAX_MSG_LEN - 1);
                        board_msg_count++;
                        
                        // Broadcast to other clients
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (client_sockets[j] > 0 && client_sockets[j] != sockfd) {
                                char broadcast_msg[MAX_MSG_LEN + 1];
                                snprintf(broadcast_msg, sizeof(broadcast_msg), "%s\n", msg);
                                write(client_sockets[j], broadcast_msg, strlen(broadcast_msg));
                            }
                        }
                    } else if (strcmp(buffer, "GET") == 0) {
                        for (int j = 0; j < BOARD_SIZE; j++) {
                            // Send only non-empty messages
                            if (strlen(board[j]) > 0) {
                                char get_msg[MAX_MSG_LEN + 1];
                                snprintf(get_msg, sizeof(get_msg), "%s\n", board[j]);
                                write(sockfd, get_msg, strlen(get_msg));
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
