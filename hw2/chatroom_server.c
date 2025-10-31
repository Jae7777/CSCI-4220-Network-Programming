
/*
 * CSCI 4220 - Assignment 2 Reference Solution
 * Concurrent Chatroom Server (select() + pthread worker pool)
 * Classic IRC-style "/me" action messages: *username text*
 *
 * This program demonstrates:
 *   - I/O multiplexing with select()
 *   - Multi-threaded worker pool using pthreads
 *   - Thread-safe producer/consumer queues
 *   - Message broadcasting to multiple clients
 *   - Basic command handling (/who, /me, /quit)
 *
 * Build:
 *   clang -Wall -Wextra -O2 -pthread chatroom_server.c -o chatroom_server.out
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_NAME     32
#define MAX_MSG      1024
#define MAX_CLIENTS  64
#define INBUF        2048

/* ---------------- Data Structures ---------------- */

typedef struct Job {
    int sender_fd;                  // The file descriptor (socket) of the client who sent the message
    char username[MAX_NAME];        // Username of the sender
    char msg[MAX_MSG];              // Raw message text sent by the client
    struct Job *next;               // Pointer to the next Job in the queue (linked-list structure)
} Job;

/*
 * Thread-safe FIFO queue structure.
 * Used for both job_queue (raw messages from clients)
 * and bcast_queue (formatted messages ready to broadcast).
 */
typedef struct Queue {
    Job *head;              // Pointer to the first Job in the queue
    Job *tail;              // Pointer to the last Job in the queue
    pthread_mutex_t mtx;    // Mutex to protect access to the queue
    pthread_cond_t cv;      // Condition variable for thread signaling
    int closed;             // Flag: 1 when queue is closed (no new Jobs)
} Queue;

static Queue job_queue, bcast_queue;

/* ---------------- Client Management ---------------- */
typedef struct Client {
    int fd;
    char username[MAX_NAME];
    char inbuf[INBUF];
    int inbuf_len;
    struct Client *next;
} Client;

static Client *clients = NULL;
static pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;
static int server_fd = -1;
static int num_workers;
static int max_clients;
static int current_clients = 0;
static volatile int shutdown_flag = 0;

/* ---------------- Function Declarations ---------------- */
static void *worker_thread(void *arg);
static void handle_new_connection(int server_fd);
static void handle_client_message(Client *client);
static void remove_client(Client *client);
static void broadcast_message(const char *msg, int exclude_fd);
static void send_to_client(int fd, const char *msg);
static int is_username_taken(const char *username);
static void to_lowercase(char *str);
static void process_message(Job *job);
static void handle_signal(int sig);

/* ---------------- Queue Utilities ---------------- */
static void q_init(Queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->closed = 0;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cv, NULL);
}
static void q_close(Queue *q) {
    pthread_mutex_lock(&q->mtx);
    q->closed = 1;
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->mtx);
}
static void q_push(Queue *q, Job *j) {
    pthread_mutex_lock(&q->mtx);
    j->next = NULL;
    
    if (q->tail == NULL) {
        q->head = q->tail = j;
    } else {
        q->tail->next = j;
        q->tail = j;
    }
    
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
}
static Job *q_pop(Queue *q) {
    pthread_mutex_lock(&q->mtx);
    
    while (q->head == NULL && !q->closed) {
        pthread_cond_wait(&q->cv, &q->mtx);
    }
    
    if (q->closed && q->head == NULL) {
        pthread_mutex_unlock(&q->mtx);
        return NULL;
    }
    
    Job *j = q->head;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    
    pthread_mutex_unlock(&q->mtx);
    return j;
}

static Job *q_try_pop(Queue *q) {
    pthread_mutex_lock(&q->mtx);
    
    if (q->head == NULL) {
        pthread_mutex_unlock(&q->mtx);
        return NULL;
    }
    
    Job *j = q->head;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    
    pthread_mutex_unlock(&q->mtx);
    return j;
}

/* ---------------- Main ---------------- */
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <num_workers> <max_clients>\n", argv[0]);
        exit(1);
    }
    
    int port = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    max_clients = atoi(argv[3]);
    
    if (port <= 0 || num_workers <= 0 || max_clients <= 0) {
        fprintf(stderr, "Invalid arguments\n");
        exit(1);
    }
    
    // signal handler for graceful shutdown
    signal(SIGINT, handle_signal);
    
    q_init(&job_queue);
    q_init(&bcast_queue);
    
    // server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    // socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    
    // bind socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    // listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("Chatroom server listening on port %d\n", port);
    printf("Workers: %d, Max clients: %d\n", num_workers, max_clients);
    
    // worker threads
    pthread_t *workers = malloc(num_workers * sizeof(pthread_t));
    for (int i = 0; i < num_workers; i++) {
        if (pthread_create(&workers[i], NULL, worker_thread, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }
    
    // select() loop
    fd_set read_fds;
    int max_fd = server_fd;
    
    while (!shutdown_flag) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        // add all client sockets to read set
        pthread_mutex_lock(&clients_mtx);
        Client *client = clients;
        while (client) {
            FD_SET(client->fd, &read_fds);
            if (client->fd > max_fd) max_fd = client->fd;
            client = client->next;
        }
        pthread_mutex_unlock(&clients_mtx);

        struct timeval timeout = {1, 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            handle_new_connection(server_fd);
        }

        pthread_mutex_lock(&clients_mtx);
        client = clients;
        while (client) {
            Client *next = client->next;
            if (FD_ISSET(client->fd, &read_fds)) {
                pthread_mutex_unlock(&clients_mtx);
                handle_client_message(client);
                pthread_mutex_lock(&clients_mtx);
            }
            client = next;
        }
        pthread_mutex_unlock(&clients_mtx);

        // process broadcast queue
        Job *bcast_job;
        while ((bcast_job = q_try_pop(&bcast_queue)) != NULL) {
            broadcast_message(bcast_job->msg, bcast_job->sender_fd);
            free(bcast_job);
        }
    }
    
    // cleanup
    printf("Shutting down server...\n");
    
    // close server socket
    if (server_fd >= 0) {
        close(server_fd);
    }
    
    // close all client connections
    pthread_mutex_lock(&clients_mtx);
    Client *client = clients;
    while (client) {
        Client *next = client->next;
        close(client->fd);
        free(client);
        client = next;
    }
    clients = NULL;
    pthread_mutex_unlock(&clients_mtx);
    
    // close queues
    if (!job_queue.closed) {
        q_close(&job_queue);
    }
    if (!bcast_queue.closed) {
        q_close(&bcast_queue);
    }
    
    // wait for all worker threads to exit
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }
    
    free(workers);
    printf("Server shutdown complete\n");
    return 0;
}

/* ---------------- Worker Thread Function ---------------- */
static void *worker_thread(void *arg) {
    (void)arg;
    
    while (!shutdown_flag) {
        Job *job = q_pop(&job_queue);
        if (job == NULL) {
            break;
        }
        
        process_message(job);
        free(job);
    }
    
    return NULL;
}

/* ---------------- Signal Handler ---------------- */
static void handle_signal(int sig) {
    (void)sig;
    printf("\nReceived SIGINT (Ctrl+C). Initiating graceful shutdown...\n");
    shutdown_flag = 1;
    
    // stop accepting new clients by closing server socket
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    
    // mark queues as closed to signal worker threads to exit
    q_close(&job_queue);
    q_close(&bcast_queue);
}

/* ---------------- Client Management Functions ---------------- */
static void handle_new_connection(int server_fd) {
    if (current_clients >= max_clients) {
        // reject connection
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd >= 0) {
            send_to_client(client_fd, "Server is full. Please try again later.\n");
            close(client_fd);
        }
        return;
    }
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_fd < 0) {
        perror("accept");
        return;
    }
    
    // create new client
    Client *new_client = malloc(sizeof(Client));
    new_client->fd = client_fd;
    new_client->username[0] = '\0';
    new_client->inbuf_len = 0;
    new_client->next = NULL;
    
    // add to client list
    pthread_mutex_lock(&clients_mtx);
    new_client->next = clients;
    clients = new_client;
    current_clients++;
    pthread_mutex_unlock(&clients_mtx);
    
    // send welcome message
    send_to_client(client_fd, "Welcome to Chatroom! Please enter your username:\n");
}

static void handle_client_message(Client *client) {
    char buffer[1024];
    int bytes_read = recv(client->fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        // client disconnected or error occurred
        remove_client(client);
        return;
    }

    buffer[bytes_read] = '\0';

    // append received data to the client's personal input buffer
    if (client->inbuf_len + bytes_read < INBUF) {
        memcpy(client->inbuf + client->inbuf_len, buffer, bytes_read);
        client->inbuf_len += bytes_read;
    } else {
        // buffer overflow, handle error (e.g., disconnect client)
        remove_client(client);
        return;
    }

    // process all complete lines (ending in '\n') from the buffer
    char *line_start = client->inbuf;
    char *newline;
    while ((newline = strchr(line_start, '\n')) != NULL) {
        *newline = '\0'; // null-terminate the line to treat it as a string

        // handle the optional '\r' for cross-platform compatibility
        if (newline > line_start && *(newline - 1) == '\r') {
            *(newline - 1) = '\0';
        }

        if (strlen(line_start) > 0) {
            // allocate and populate the job struct
            Job *job = malloc(sizeof(Job));
            if (job == NULL) {
                perror("malloc failed");
                continue;
            }
            job->sender_fd = client->fd;
            strncpy(job->username, client->username, MAX_NAME - 1);
            job->username[MAX_NAME-1] = '\0';

            strncpy(job->msg, line_start, MAX_MSG - 1);
            job->msg[MAX_MSG - 1] = '\0';

            q_push(&job_queue, job);
        }

        // move to the start of the next potential line
        line_start = newline + 1;
    }

    // move any remaining partial message to the beginning of the buffer
    int remaining_len = client->inbuf_len - (line_start - client->inbuf);
    if (remaining_len > 0) {
        memmove(client->inbuf, line_start, remaining_len);
    }
    client->inbuf_len = remaining_len;
}

static void remove_client(Client *client) {
    char username_copy[MAX_NAME] = {0};
    int fd_to_close = client->fd;

    pthread_mutex_lock(&clients_mtx);

    // remove client from the linked list
    if (clients == client) {
        clients = client->next;
    } else {
        Client *prev = clients;
        while (prev && prev->next != client) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = client->next;
        }
    }

    current_clients--;

    // copy username so we can broadcast the message *after* unlocking
    if (strlen(client->username) > 0) {
        strncpy(username_copy, client->username, MAX_NAME - 1);
    }

    // free the client's resources while still under the lock
    close(fd_to_close);
    free(client);

    pthread_mutex_unlock(&clients_mtx);

    // broadcast the leave message outside the critical section
    if (strlen(username_copy) > 0) {
        char leave_msg[256];
        snprintf(leave_msg, sizeof(leave_msg), "%s has left the chat.\n", username_copy);
        broadcast_message(leave_msg, -1);
    }
}

static void broadcast_message(const char *msg, int exclude_fd) {
    pthread_mutex_lock(&clients_mtx);
    
    Client *client = clients;
    while (client) {
        if (client->fd != exclude_fd) {
            send_to_client(client->fd, msg);
        }
        client = client->next;
    }
    
    pthread_mutex_unlock(&clients_mtx);
}

static void send_to_client(int fd, const char *msg) {
    int len = strlen(msg);
    int sent = 0;
    
    while (sent < len) {
        int bytes = send(fd, msg + sent, len - sent, 0);
        if (bytes <= 0) {
            break;
        }
        sent += bytes;
    }
}

static int is_username_taken(const char *username) {
    pthread_mutex_lock(&clients_mtx);
    
    Client *client = clients;
    while (client) {
        if (strcasecmp(client->username, username) == 0) {
            pthread_mutex_unlock(&clients_mtx);
            return 1;
        }
        client = client->next;
    }
    
    pthread_mutex_unlock(&clients_mtx);
    return 0;
}

static void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

/* ---------------- Message Processing Function ---------------- */
static void process_message(Job *job) {
    // find the client who sent this message
    pthread_mutex_lock(&clients_mtx);
    Client *sender = NULL;
    Client *client = clients;
    while (client) {
        if (client->fd == job->sender_fd) {
            sender = client;
            break;
        }
        client = client->next;
    }
    pthread_mutex_unlock(&clients_mtx);
    
    if (sender == NULL) {
        return;
    }
    
    // handle username setup
    if (strlen(sender->username) == 0) {
        // username message
        char username[MAX_NAME];
        strncpy(username, job->msg, MAX_NAME - 1);
        username[MAX_NAME - 1] = '\0';
        
        // validate username (letters, digits, underscores only)
        int valid = 1;
        for (int i = 0; username[i]; i++) {
            if (!isalnum(username[i]) && username[i] != '_') {
                valid = 0;
                break;
            }
        }
        
        if (!valid || strlen(username) == 0) {
            send_to_client(sender->fd, "Invalid username. Use letters, digits, or underscores only.\n");
            send_to_client(sender->fd, "Please enter your username:\n");
            return;
        }
        
        // check if username is taken
        if (is_username_taken(username)) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Username \"%s\" is already in use. Try another:\n", username);
            send_to_client(sender->fd, error_msg);
            return;
        }
        
        // set username
        strncpy(sender->username, username, MAX_NAME - 1);
        sender->username[MAX_NAME - 1] = '\0';
        
        // send private welcome message
        char welcome_msg[256];
        snprintf(welcome_msg, sizeof(welcome_msg), "Let's start chatting, %s!\n", username);
        send_to_client(sender->fd, welcome_msg);

        // create and enqueue a public "joined" message for broadcast
        char join_msg[256];
        snprintf(join_msg, sizeof(join_msg), "%s joined the chat.\n", username);

        Job *bcast_job = malloc(sizeof(Job));
        bcast_job->sender_fd = sender->fd; // exclude the new client from the broadcast
        strncpy(bcast_job->msg, join_msg, MAX_MSG - 1);
        bcast_job->msg[MAX_MSG - 1] = '\0';
        q_push(&bcast_queue, bcast_job);
        
        return;
    }
    
    // process regular message or command
    char *msg = job->msg;
    
    // remove trailing newlines
    while (strlen(msg) > 0 && (msg[strlen(msg) - 1] == '\n' || msg[strlen(msg) - 1] == '\r')) {
        msg[strlen(msg) - 1] = '\0';
    }
    
    // handle commands
    if (msg[0] == '/') {
        char cmd[256];
        char args[1024] = {0};
        
        // parse command and arguments
        if (sscanf(msg, "%255s %1023[^\n]", cmd, args) < 1) {
            send_to_client(sender->fd, "Invalid command. Type /who, /me, or /quit.\n");
            return;
        }
        
        to_lowercase(cmd);
        
        if (strcmp(cmd, "/who") == 0) {
            // list all connected users
            pthread_mutex_lock(&clients_mtx);
            char who_msg[2048] = "Active users:\n";
            client = clients;
            while (client) {
                if (strlen(client->username) > 0) {
                    strncat(who_msg, " - ", sizeof(who_msg) - strlen(who_msg) - 1);
                    strncat(who_msg, client->username, sizeof(who_msg) - strlen(who_msg) - 1);
                    strncat(who_msg, "\n", sizeof(who_msg) - strlen(who_msg) - 1);
                }
                client = client->next;
            }
            pthread_mutex_unlock(&clients_mtx);
            
            send_to_client(sender->fd, who_msg);
            
        } else if (strcmp(cmd, "/me") == 0) {
            // action message
            if (strlen(args) == 0) {
                send_to_client(sender->fd, "Usage: /me <action>\n");
                return;
            }
            
            char action_msg[2048];
            snprintf(action_msg, sizeof(action_msg), "*%s %s*\n", sender->username, args);
            
            // create broadcast job
            Job *bcast_job = malloc(sizeof(Job));
            bcast_job->sender_fd = sender->fd;
            strncpy(bcast_job->username, sender->username, MAX_NAME - 1);
            bcast_job->username[MAX_NAME - 1] = '\0';
            strncpy(bcast_job->msg, action_msg, MAX_MSG - 1);
            bcast_job->msg[MAX_MSG - 1] = '\0';
            
            q_push(&bcast_queue, bcast_job);
            
        } else if (strcmp(cmd, "/quit") == 0) {
            close(sender->fd);
            return; 
        } else {
            send_to_client(sender->fd, "Invalid command. Type /who, /me, or /quit.\n");
        }
        
    } else {
        // regular message
        char formatted_msg[2048];
        snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s\n", sender->username, msg);
        
        // create broadcast job
        Job *bcast_job = malloc(sizeof(Job));
        bcast_job->sender_fd = sender->fd;
        strncpy(bcast_job->username, sender->username, MAX_NAME - 1);
        bcast_job->username[MAX_NAME - 1] = '\0';
        strncpy(bcast_job->msg, formatted_msg, MAX_MSG - 1);
        bcast_job->msg[MAX_MSG - 1] = '\0';
        
        q_push(&bcast_queue, bcast_job);
    }
}