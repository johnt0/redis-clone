#!/bin/bash

# 1. Cleanup old processes
echo "Cleaning up old server/client..."
pkill redis_server
pkill redis_client
rm -f server.log

# 2. Build the project
echo "Compiling project..."
mkdir -p build
cd build
cmake .. -DCMAKE_CXX_STANDARD=20
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi
cd ..

# 3. Launch Server in the background
echo "Launching Server on port 6379..."
./build/redis_server 6379 > server.log 2>&1 &
SERVER_PID=$!

# 4. Wait a moment for server to bind the port
sleep 1

# 5. Launch Client in the foreground
echo "Launching Client..."
echo "------------------------------------------"
./build/redis_client 127.0.0.1 6379

# 6. Once client exits, kill the server
echo "------------------------------------------"
echo "Shutting down server (PID: $SERVER_PID)..."
kill $SERVER_PID