#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_NUM_THREADS 4
#define MAX_MSG_LEN 1024

// for each worker thread
typedef struct {
    int tid;           // index (1..T)
    int start;         // start of Fibonacci range
    int end;           // end of Fibonacci range
    long sum;        
    char result[256];  
} fib_data_t;

// handler thread data
typedef struct {
    int connfd;        // client socket
    int client_num;    // client num for logging
    int num_threads;  
} client_data_t;

int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

// Iterative Fibonacci computation
long fib(int n) {
    if (n <= 2) return 1;
    long a = 1, b = 1, c;
    for (int i = 3; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

// Worker thread function
void* worker_thread(void* arg) {
    fib_data_t* data = (fib_data_t*)arg;
    
    data->sum = 0;
    data->result[0] = '\0';
    
    char temp[32];
    int first = 1;
    
    // Compute Fibonacci for assigned range
    for (int i = data->start; i <= data->end; i++) {
        long fib_val = fib(i);
        data->sum += fib_val;
        
        // Append to result string
        if (!first) {
            strcat(data->result, " ");
        }
        snprintf(temp, sizeof(temp), "%ld", fib_val);
        strcat(data->result, temp);
        first = 0;
    }
    
    return (void*)data;
}

// Client handler thread function
void* client_handler(void* arg) {
    client_data_t* client_data = (client_data_t*)arg;
    int connfd = client_data->connfd;
    int client_num = client_data->client_num;
    int num_threads = client_data->num_threads;
    
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    getpeername(connfd, (struct sockaddr*)&cliaddr, &len);
    int client_port = ntohs(cliaddr.sin_port);
    
    printf("[INFO] New client from port %d\n", client_port);
    
    // send prompt to client
    char prompt[256];
    snprintf(prompt, sizeof(prompt), "Connected from %d. Please enter an integer N:\n", client_port);
    write(connfd, prompt, strlen(prompt));
    
    // read N from client
    char buffer[MAX_MSG_LEN];
    int n = read(connfd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        printf("[Client %d] Failed to read N\n", client_num);
        close(connfd);
        free(client_data);
        return NULL;
    }
    
    buffer[n] = '\0';
    int N = atoi(buffer);
    
    if (N <= 0) {
        printf("[Client %d] Invalid N = %d\n", client_num, N);
        close(connfd);
        free(client_data);
        return NULL;
    }
    
    printf("[Client %d] Received N = %d\n", client_num, N);
    
    // create worker threads
    pthread_t threads[num_threads];
    fib_data_t thread_data[num_threads];
    
    // divide range [1..N] among threads
    int range_per_thread = N / num_threads;
    int remainder = N % num_threads;
    
    int current_start = 1;
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].tid = i + 1;
        thread_data[i].start = current_start;
        
        // distribute remainder among first threads
        int current_range = range_per_thread + (i < remainder ? 1 : 0);
        thread_data[i].end = current_start + current_range - 1;
        
        current_start = thread_data[i].end + 1;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }
    
    // wait for all threads and collect results
    long total_sum = 0;
    char response[4096] = "";
    
    for (int i = 0; i < num_threads; i++) {
        void* ret_val;
        pthread_join(threads[i], &ret_val);
        fib_data_t* result = (fib_data_t*)ret_val;
        
        // log thread result
        printf("[Client %d][Thread %d] Range [%d–%d]: sum=%ld\n",
               client_num, result->tid, result->start, result->end, result->sum);
        
        // format output line for client
        char line[512];
        snprintf(line, sizeof(line), "T%d: [%d–%d] -> %s\n",
                result->tid, result->start, result->end, result->result);
        strcat(response, line);
        
        total_sum += result->sum;
    }
    
    // add summary to response
    char summary[256];
    snprintf(summary, sizeof(summary), 
             "Total computed = %d Fibonacci numbers\nSum = %ld\n", N, total_sum);
    strcat(response, summary);
    
    printf("[Client %d] Total sum = %ld\n", client_num, total_sum);
    
    // send response to client
    write(connfd, response, strlen(response));
    
    // close connection
    close(connfd);
    free(client_data);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <port> [num_threads]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    int num_threads = DEFAULT_NUM_THREADS;
    
    if (argc == 3) {
        num_threads = atoi(argv[2]);
        if (num_threads <= 0) {
            fprintf(stderr, "Invalid number of threads\n");
            exit(EXIT_FAILURE);
        }
    }
    
    // create listening socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    
    // set socket options
    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // bind to port
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    
    // listen for connections
    if (listen(listenfd, 10) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }
    
    printf("[INFO] Server listening on port %d\n", port);
    
    // accept connections in a loop
    while (1) {
        // todo: process connections and spawn threads
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            perror("accept error");
            continue;
        }
        
        // allocate client data
        client_data_t* client_data = malloc(sizeof(client_data_t));
        client_data->connfd = connfd;
        client_data->num_threads = num_threads;
        
        pthread_mutex_lock(&counter_mutex);
        client_data->client_num = ++client_counter;
        pthread_mutex_unlock(&counter_mutex);
        
        // spawn client handler thread
        pthread_t thread;
        pthread_create(&thread, NULL, client_handler, client_data);
        pthread_detach(thread);  // detach so resources are freed automatically
    }
    
    close(listenfd);
    return 0;
}

