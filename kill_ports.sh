#!/bin/bash

# Define the ports your app uses
PORTS=(6379 8080)

echo "Checking for processes on ports: ${PORTS[*]}"

for PORT in "${PORTS[@]}"; do
    # Find the PID (Process ID) using the port
    PID=$(lsof -t -i:$PORT)

    if [ -z "$PID" ]; then
        echo "Port $PORT is already free."
    else
        echo "Port $PORT is being used by PID: $PID. Killing it..."
        # Kill the process
        kill -9 $PID
        
        # Double check
        sleep 0.5
        if lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null ; then
            echo "Failed to kill process on port $PORT"
        else
            echo "Port $PORT successfully cleared."
        fi
    fi
done