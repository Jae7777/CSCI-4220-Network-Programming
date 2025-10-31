#!/bin/bash

echo "Compiling test client..."
gcc -o test_client test_client.c

echo "Starting server in background..."
./chatroom_server.out 12000 3 5 &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo "Testing client connection..."
./test_client

echo "Stopping server..."
kill $SERVER_PID

echo "Test complete!"
