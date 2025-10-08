#!/bin/bash

# Test script for Lab 4 server

PORT=9000
NUM_THREADS=4

echo "==================================="
echo "Lab 4 Server Test Script"
echo "==================================="
echo ""

# Check if server binary exists
if [ ! -f "./lab4" ]; then
    echo "Error: lab4 binary not found. Run 'make' first."
    exit 1
fi

# Start the server in background
echo "Starting server on port $PORT with $NUM_THREADS threads..."
./lab4 $PORT $NUM_THREADS > server.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 1

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Error: Server failed to start"
    cat server.log
    exit 1
fi

echo "Server started successfully!"
echo ""

# Test cases
test_values=(12 20 5 1 10)

for N in "${test_values[@]}"; do
    echo "-----------------------------------"
    echo "Testing with N = $N"
    echo "-----------------------------------"
    echo "$N" | nc 127.0.0.1 $PORT
    echo ""
    sleep 0.5
done

echo "==================================="
echo "All tests completed!"
echo "==================================="
echo ""
echo "Server log:"
echo "-----------------------------------"
cat server.log
echo "-----------------------------------"
echo ""

# Stop the server
echo "Stopping server (PID: $SERVER_PID)..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "Done!"

