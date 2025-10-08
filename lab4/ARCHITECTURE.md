# Lab 4 Architecture

## System Overview

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Client 1   │     │  Client 2   │     │  Client 3   │
│ (netcat/    │     │ (netcat/    │     │ (netcat/    │
│  telnet)    │     │  telnet)    │     │  telnet)    │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │ TCP Connection    │                   │
       └───────────┬───────┴───────────┬───────┘
                   │                   │
              ┌────▼───────────────────▼────┐
              │      Main Server Thread      │
              │    (Accept Connections)      │
              │       Port 9000              │
              └────┬───────────────────┬─────┘
                   │                   │
           spawn   │                   │  spawn
        thread 1   │                   │  thread 2
                   │                   │
         ┌─────────▼─────┐   ┌────────▼──────┐
         │ Client Handler │   │ Client Handler│
         │   Thread 1     │   │   Thread 2    │
         │  (for Client1) │   │ (for Client2) │
         └────────┬────────┘   └───────┬───────┘
                  │                    │
                  │ Creates            │
                  │ Worker             │
                  │ Threads            │
                  │                    │
    ┌─────────────┼────────────┐       │
    │             │            │       │
┌───▼───┐   ┌────▼────┐  ┌───▼────┐  ...
│Worker │   │ Worker  │  │ Worker │
│Thread │   │ Thread  │  │ Thread │
│  1    │   │   2     │  │   3    │
│[1-3]  │   │ [4-6]   │  │ [7-9]  │
└───────┘   └─────────┘  └────────┘
Fib(1-3)     Fib(4-6)     Fib(7-9)
```

## Thread Architecture

### Two-Level Threading Model

1. **Main Thread** (always running)
   - Listens for incoming connections
   - Accepts new clients
   - Spawns client-handler threads

2. **Client-Handler Threads** (one per connected client)
   - Reads N from client
   - Creates worker threads
   - Waits for worker completion (pthread_join)
   - Aggregates results
   - Sends response to client
   - Thread terminates after client disconnects

3. **Worker Threads** (num_threads per client)
   - Computes Fibonacci numbers for assigned range
   - Returns results via pthread_exit
   - Thread terminates after computation

## Data Flow

```
Client connects
     │
     ▼
Main thread accepts → spawn Client Handler Thread
                              │
                              ▼
                        Read N from client
                              │
                              ▼
                        Divide [1..N] into ranges
                              │
                              ▼
                   ┌──────────┼──────────┐
                   │          │          │
              Spawn W1    Spawn W2    Spawn W3
                   │          │          │
              Compute    Compute    Compute
              Fib(1-4)   Fib(5-8)   Fib(9-12)
                   │          │          │
                   ▼          ▼          ▼
              pthread_exit(result1)
                            pthread_exit(result2)
                                      pthread_exit(result3)
                   │          │          │
                   └──────────┼──────────┘
                              ▼
                        pthread_join all
                              │
                              ▼
                        Aggregate results
                              │
                              ▼
                        Send to client
                              │
                              ▼
                        Close connection
                              │
                              ▼
                        Handler terminates
```

## Key Data Structures

### fib_data_t (Worker Thread Data)
```c
typedef struct {
    int tid;           // Thread ID (1, 2, 3, ...)
    int start;         // First Fibonacci index
    int end;           // Last Fibonacci index
    long sum;          // Sum of computed values
    char result[256];  // Space-separated Fibonacci numbers
} fib_data_t;
```

**Example for N=12, 4 threads:**
```
Thread 1: tid=1, start=1,  end=3,  result="1 1 2",      sum=4
Thread 2: tid=2, start=4,  end=6,  result="3 5 8",      sum=16
Thread 3: tid=3, start=7,  end=9,  result="13 21 34",   sum=68
Thread 4: tid=4, start=10, end=12, result="55 89 144",  sum=287
```

### client_data_t (Handler Thread Data)
```c
typedef struct {
    int connfd;        // Client socket file descriptor
    int client_num;    // Unique client identifier
    int num_threads;   // Number of worker threads to create
} client_data_t;
```

## Range Distribution Algorithm

For N=12 and num_threads=4:
```
range_per_thread = 12 / 4 = 3
remainder = 12 % 4 = 0

Thread 1: [1  - 3 ] → 3 values
Thread 2: [4  - 6 ] → 3 values
Thread 3: [7  - 9 ] → 3 values
Thread 4: [10 - 12] → 3 values
```

For N=13 and num_threads=4:
```
range_per_thread = 13 / 4 = 3
remainder = 13 % 4 = 1

Thread 1: [1  - 4 ] → 4 values (gets +1 from remainder)
Thread 2: [5  - 7 ] → 3 values
Thread 3: [8  - 10] → 3 values
Thread 4: [11 - 13] → 3 values
```

## Thread Synchronization

### No Shared Data Between Workers
- Each worker thread operates on its own range
- No need for mutex/locks between workers
- Independent computation = true parallelism

### Synchronization Points
1. **Client counter** - Protected by mutex
   ```c
   pthread_mutex_lock(&counter_mutex);
   client_num = ++client_counter;
   pthread_mutex_unlock(&counter_mutex);
   ```

2. **Join barrier** - Handler waits for all workers
   ```c
   for (int i = 0; i < num_threads; i++) {
       pthread_join(threads[i], &ret_val);
   }
   ```

## Memory Management

### Heap Allocation
- `client_data_t` allocated with malloc
- Freed by client handler thread after use

### Stack Allocation
- `fib_data_t` array on handler's stack
- Automatically freed when handler returns

### Thread Resources
- Client handler: Detached (pthread_detach)
- Worker threads: Joined (pthread_join)

## Performance Characteristics

### Time Complexity
- Single-threaded: O(N) for N Fibonacci numbers
- Multi-threaded: O(N/T) where T = num_threads
- Speedup: ~T (linear with number of threads)

### Space Complexity
- Per worker: O(1) for computation
- Per worker: O(R) for result string, R = range size
- Total: O(T) space for T threads

### Scalability
- Can handle multiple clients simultaneously
- Each client gets independent thread pool
- Limited by system thread limits and CPU cores

## Example Execution Timeline

```
Time  Main Thread     Client1 Handler    Worker1    Worker2
----  -----------     ---------------    -------    -------
0ms   listen(9000)
10ms  accept() → 
      spawn →         start
                      read N=8
20ms                  create workers →   compute    compute
                                         Fib(1-4)   Fib(5-8)
30ms                                     ↓          ↓
40ms                  join() ←           done       done
                      aggregate
50ms                  send response
                      close socket
                      terminate
60ms  accept() ...
```

## Debugging Tips

### Server-side logging
```
[INFO] Server listening on port 9000          ← Main thread
[INFO] New client from port 52000             ← Handler spawn
[Client 1] Received N = 12                    ← Handler read N
[Client 1][Thread 1] Range [1–3]: sum=4       ← Worker1 complete
[Client 1][Thread 2] Range [4–6]: sum=16      ← Worker2 complete
[Client 1][Thread 3] Range [7–9]: sum=68      ← Worker3 complete
[Client 1][Thread 4] Range [10–12]: sum=287   ← Worker4 complete
[Client 1] Total sum = 376                    ← Handler aggregate
```

### Thread lifecycle
```
Main Thread:        [==============================...]
Client Handler 1:      [====]
Worker 1-1:              [=]
Worker 1-2:              [=]
Worker 1-3:              [=]
Worker 1-4:              [=]
Client Handler 2:           [====]
Worker 2-1:                   [=]
Worker 2-2:                   [=]
...
```

## Comparison with Lab 3

| Feature | Lab 3 | Lab 4 |
|---------|-------|-------|
| Model | I/O multiplexing (select) | Multi-threading |
| Concurrency | Single thread | Multiple threads |
| Scalability | Limited by select | Limited by threads |
| Computation | N/A | Parallel Fibonacci |
| Blocking | Non-blocking | Blocking OK |
| Complexity | Higher (state mgmt) | Lower (simpler) |

## Further Optimizations (Optional)

1. **Thread Pool** - Pre-create threads instead of spawning
2. **Load Balancing** - Dynamic work stealing
3. **Memoization** - Cache Fibonacci results
4. **Long Integer** - Use GMP for large N
5. **Connection Pool** - Reuse connections

