#!/bin/bash

# Remove old FIFO if it exists
rm -f register_fifo

# Start the server in the background
./server/kvs ./server/jobs 3 3 register_fifo &
SERVER_PID=$!  # Save the server PID

# Start clients in the background
./client/client c2 register_fifo < ./client/client_tests.txt &
./client/client c3 register_fifo < ./client/client_tests2.txt &

# Wait for the first set of clients to finish
#wait
sleep 2

# Send SIGUSR1 to the server to trigger cleanup logic
echo "Sending SIGUSR1 to the server..."
kill -SIGUSR1 $SERVER_PID &

# Give the server time to process the signal
sleep 2

# Start new clients after SIGUSR1
echo "Starting new clients after SIGUSR1... OUTRA CENA" 
./client/client c4 register_fifo < ./client/client_tests2.txt &
./client/client c5 register_fifo < ./client/client_tests.txt &

# Wait for the new clients to finish
#wait
sleep 20

# Optional: Gracefully terminate the server (e.g., with SIGTERM)
kill -SIGTERM $SERVER_PID
