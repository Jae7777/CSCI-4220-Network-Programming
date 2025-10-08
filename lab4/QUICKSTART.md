# Lab 4 Quick Start Guide

## Build and Run in 3 Steps

### 1. Compile
```bash
cd lab4
make
```

### 2. Start Server
```bash
./lab4 9000
```
Or with custom number of worker threads:
```bash
./lab4 9000 8
```

### 3. Test with Client

**Option A - Using netcat:**
```bash
nc 127.0.0.1 9000
# Then type a number, e.g., 12
```

**Option B - Using the test client:**
```bash
./client_test 127.0.0.1 9000
# Follow the prompts
```

**Option C - Using the automated test script:**
```bash
./test_server.sh
```

## Expected Output

### Server Output:
```
[INFO] Server listening on port 9000
[INFO] New client from port 52000
[Client 1] Received N = 12
[Client 1][Thread 1] Range [1–3]: sum=4
[Client 1][Thread 2] Range [4–6]: sum=16
[Client 1][Thread 3] Range [7–9]: sum=68
[Client 1][Thread 4] Range [10–12]: sum=287
[Client 1] Total sum = 376
```

### Client Output:
```
Connected from 52000. Please enter an integer N:
12
T1: [1–3] -> 1 1 2
T2: [4–6] -> 3 5 8
T3: [7–9] -> 13 21 34
T4: [10–12] -> 55 89 144
Total computed = 12 Fibonacci numbers
Sum = 376
```

## Understanding the Output

- **T1, T2, T3, T4**: Thread identifiers
- **[1-3], [4-6], etc.**: Range of Fibonacci indices each thread computes
- **1 1 2, 3 5 8, etc.**: The actual Fibonacci values computed
- **Sum**: Total sum of all Fibonacci numbers from Fib(1) to Fib(N)

## How It Works

1. Server listens on specified port
2. When client connects, server spawns a **client-handler thread**
3. Handler reads integer N from client
4. Handler spawns **worker threads** (default: 4)
5. Each worker computes Fibonacci numbers for its assigned range
6. Handler aggregates results and sends response to client
7. Connection closes, server continues accepting new clients

## Testing Multiple Clients

The server supports multiple concurrent clients. Open several terminals:

**Terminal 1:**
```bash
./lab4 9000
```

**Terminal 2:**
```bash
nc 127.0.0.1 9000
12
```

**Terminal 3:**
```bash
nc 127.0.0.1 9000
20
```

Each client is handled independently!

## Assignment Requirements Checklist

✅ Multi-client TCP server  
✅ Command line: `./lab4 <port> [num_threads]`  
✅ Default num_threads = 4  
✅ Client-handler threads (one per client)  
✅ Worker threads (multiple per client)  
✅ Thread data structure with tid, start, end, sum, result  
✅ Iterative Fibonacci computation O(n)  
✅ Parallel range decomposition  
✅ Result aggregation  
✅ Formatted output to client  
✅ Server logging to stdout  

## Common Issues

**"Address already in use"**
- Wait ~60 seconds for OS to release the port
- Use a different port: `./lab4 9001`
- Kill existing server: `killall lab4`

**"Connection refused"**
- Make sure server is running first
- Check you're using the correct port

**Incorrect sum**
- This implementation uses Fib(1)=1, Fib(2)=1
- Sum of Fib(1) through Fib(12) = 376

## Submission

Submit `lab4.c` with:
```bash
gcc -o lab4 -lm lab4.c libunp.a
```

**Note:** This implementation doesn't require libunp.a, but if your assignment requires it, uncomment the alternative build command in the Makefile.

## Files Overview

- `lab4.c` - Main server (submit this)
- `client_test.c` - Test client helper
- `test_server.sh` - Automated testing script
- `Makefile` - Build script
- `README.md` - Detailed documentation
- `QUICKSTART.md` - This file

## Need Help?

Check the detailed README.md for:
- Implementation details
- Thread data structures
- Algorithm explanation
- Troubleshooting guide

