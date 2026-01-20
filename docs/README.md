# High-Performance Concurrent Redis Clone

A thread-safe, high-performance Key-Value store built in C++20, designed with a focus on memory safety, strict type correctness, and efficient resource management.

## Overview

A minimalist clone of Redis that implements the RESP (REdis Serialization Protocol). Unlike basic implementations, RediSafe utilizes a Monitor Pattern for thread safety and a dual-index expiry system to maintain high-read throughput while preventing memory bloat.

### Architectural Decisions

* **Monotonic Time:** Utilizes std::chrono::steady_clock for all TTL (Time-to-Live) calculations to prevent clock-drift issues associated with system time adjustments.
* **Shared Mutex Locking:** Implements std::shared_mutex to allow concurrent GET requests while ensuring atomic SET operations via exclusive locking.
* **Ownership Semantics:** Leverages C++ move semantics to minimize buffer copying during network-to-store transfers, ensuring memory efficiency.
* **The Expiry Index:** Decouples persistent data from volatile data using a secondary index to optimize background cleanup cycles.

---

## Project Structure

```text
.
├── src/
│   ├── common/           # Type aliases (i64, u32) and global constants
│   ├── server/           # TCPServer and SafeStore (ConcurrentStore) logic
│   ├── util/             # RESP parser and serializer
│   └── main.cpp          # Application entry point
└── README.md