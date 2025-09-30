#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_LINE 1024

void handle_server_message(int sockfd) {
    char buf[MAX_LINE];
    int n = read(sockfd, buf, MAX_LINE - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        fflush(stdout);
    } else {
        printf("Server closed connection\n");
        exit(EXIT_SUCCESS);
    }
}

void handle_user_input(int sockfd) {
    char buf[MAX_LINE];
    if (fgets(buf, MAX_LINE, stdin) != NULL) {
        write(sockfd, buf, strlen(buf));
    } else { // EOF
        shutdown(sockfd, SHUT_WR);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[1]);
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server on port %d\n", server_port);

    fd_set rset;
    FD_ZERO(&rset);
    int maxfdp1 = (STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd) + 1;

    for (;;) {
        FD_SET(STDIN_FILENO, &rset);
        FD_SET(sockfd, &rset);

        if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(sockfd, &rset)) {
            handle_server_message(sockfd);
        }

        if (FD_ISSET(STDIN_FILENO, &rset)) {
            handle_user_input(sockfd);
        }
    }

    return 0;
}
