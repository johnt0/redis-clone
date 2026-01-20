# Stage 1: Build with GCC 13
FROM gcc:13 AS builder

# Install CMake
RUN apt-get update && apt-get install -y cmake

WORKDIR /usr/src/redis_clone
COPY . .

# 1. Build ALL targets (server, client, and tests) 
# We remove '--target redis_server' so it builds everything in CMakeLists.txt
RUN cmake -B build -S . \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON && \
    cmake --build build

# Stage 2: Runtime
FROM ubuntu:24.04

# Install libstdc++ and netcat (for the healthcheck)
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/src/redis_clone/build/redis_server .
COPY --from=builder /usr/src/redis_clone/build/redis_client .

EXPOSE 6379

CMD ["./redis_server"]