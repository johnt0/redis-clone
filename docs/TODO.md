# Redis Clone Implementation - TODO List

## 1. Structural Refactor & Type Safety
- [x] **Relocate `struct Value`**
    - Move from `tcp_server.hpp` to `common/types.hpp`.
    - *Rationale:* Decouples data modeling from networking; allows unit tests to access `Value` without linking the entire server.
- [x] **Expand `common/types.hpp`**
    - [x] Add `#include <variant>` and `#include <deque>`.
    - [x] Define `using RedisList = std::deque<std::string>;`.
    - [x] Define `using RedisData = std::variant<std::string, RedisList>;`.
    - [x] Update `struct Value` to use `RedisData data;` instead of a raw string.
    - [x] Add helper methods to `Value`: `is_string()`, `is_list()`.



## 2. Update ConcurrentStore Logic
- [x] **Adapt `set()` and `get()`**
    - [x] Update `set()` to wrap the string in `RedisData`.
    - [x] Update `get()` to use `std::get_if<std::string>(&it->second.data)` to prevent crashes if the key points to a List.
- [ ] **Implement `rpush(key, values)`**
    - [ ] Handle Mutex locking (`std::unique_lock`).
    - [ ] Check if key exists:
        - If exists: Verify it is a `RedisList`. Return error code if it's a String.
        - If missing: Initialize a new `RedisList`.
    - [ ] Append all values to the back of the `deque`.
    - [ ] Return new list size.



## 3. TCPServer Command Handling
- [ ] **Route RPUSH Command**
    - [ ] Add `else if (command == "RPUSH")` to `execute_command`.
- [ ] **Implement `handle_rpush`**
    - [ ] Validate argument count (at least 3 tokens: `RPUSH`, `key`, `val1`).
    - [ ] Slicing: Collect all tokens from `index 2` to the end as values.
    - [ ] Execute store operation and return RESP Integer (`:size\r\n`).
    - [ ] Handle `WRONGTYPE` error RESP if store returns error code.

## 4. Testing & Quality Assurance
- [ ] **Update `test.cpp`**
    - [ ] **Unit Test:** Verify `RPUSH` on empty key creates list and returns 1.
    - [ ] **Unit Test:** Verify `RPUSH` with multiple arguments (e.g., `RPUSH k v1 v2 v3`) returns 3.
    - [ ] **Type Safety Test:** Verify `SET key val` followed by `RPUSH key val2` returns an error.
    - [ ] **Regression Test:** Ensure `active_expiry_cycle` still cleans up Lists correctly.

## 5. Maintenance
- [ ] Clean build: `rm -rf build/* && cmake -B build && cmake --build build`.
- [ ] Verify no "Undefined Reference" linker errors after the `Value` move.