#!/bin/bash

# Function to cleanup and terminate the server
cleanup() {
    echo "Terminating bg server..."
    pkill -f "./server"
}

# Trap the termination signal (SIGTERM) and run cleanup function
trap 'cleanup' SIGINT

echo "Running bg server..."
./server &
echo "Running client"
sleep 1
./client "$1"

wait

