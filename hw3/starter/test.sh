#!/bin/bash

cd /home/ustin/CSCI-4220-Network-Programming/hw3/starter

# Kill any existing routers
pkill -9 router
sleep 1

# Clean logs
rm -f /tmp/r*.log

# Start routers
./router configs/r1.conf > /tmp/r1.log 2>&1 &
./router configs/r2.conf > /tmp/r2.log 2>&1 &
./router configs/r3.conf > /tmp/r3.log 2>&1 &

echo "Waiting for convergence..."
sleep 12

echo "=== DV Convergence (R1) ==="
tail -8 /tmp/r1.log

echo ""
echo "=== Sending test packet ==="
./sendpkt 12001 192.168.10.10 10.0.30.55 8 "hello LPM world"
sleep 2

echo ""
echo "=== Forwarding Logs ==="
echo "[R1]:"
grep "FWD\|DELIVER" /tmp/r1.log || echo "(no forwarding)"
echo "[R2]:"
grep "FWD\|DELIVER" /tmp/r2.log || echo "(no forwarding)"
echo "[R3]:"
grep "FWD\|DELIVER" /tmp/r3.log || echo "(no delivery)"

echo ""
echo "=== Testing Neighbor Timeout ==="
echo "Killing R2..."
pkill -f "router configs/r2.conf"
echo "Waiting 16 seconds for timeout..."
sleep 16

echo "[R1] after timeout:"
tail -15 /tmp/r1.log | grep "neighbor-dead" -A 5 || echo "(no timeout detected)"

# Cleanup
pkill -9 router

