#include "../server/tcp_server.hpp"
#include "common/types.hpp"
#include "util/RESP.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
// #include <deque>
#include <format>
#include <iostream>
// #include <mutex>
#include <netinet/in.h>
#include <optional>
#include <sys/socket.h>
#include <thread>
#include <type_traits>
#include <unistd.h>

std::atomic<int> client_id_counter{0};

namespace Redis {

TCPServer::TCPServer(const std::string &address, int port)
    : host_(address), port_(port), running_(false) {}

TCPServer::~TCPServer() { stop(); }

void TCPServer::execute_command(const std::vector<std::string> &tokens,
                                int client_fd) {
  if (tokens.empty())
    return;

  std::string command = tokens[0];

  if (command == "PING") {
    handle_ping(client_fd);
  } else if (command == "ECHO") {
    handle_echo(tokens, client_fd);
  } else if (command == "SET") {
    handle_set(tokens, client_fd);
  } else if (command == "GET") {
    handle_get(tokens, client_fd);
  } else if (command == "RPUSH") {
    handle_rpush(tokens, client_fd);
  } else {
    std::cout << "Unknown command: " << command << "\n";
  }
}

void TCPServer::handle_ping(int client_fd) {
  RESP response{.resp_type = RESP::type::SIMPLE_STRING, .str = "PONG"};
  std::string serialized = serialize_RESP(response);
  robust_send(client_fd, serialized.c_str(), serialized.size());
}

void TCPServer::handle_echo(const std::vector<std::string> &tokens,
                            int client_fd) {
  if (tokens.size() < 2)
    return;

  RESP response{.resp_type = RESP::type::BULK_STRING, .str = tokens[1]};
  std::string serialized = serialize_RESP(response);
  robust_send(client_fd, serialized.c_str(), serialized.size());
}

void TCPServer::handle_set(const std::vector<std::string> &tokens,
                           int client_fd) {
  if (tokens.size() < 3)
    return;

  std::string key = tokens[1];
  std::string value = tokens[2];
  i64 ttl_ms = -1;

  if (tokens.size() >= 5) {
    try {
      std::string opt = tokens[3];
      i64 amount = std::stoll(tokens[4]);
      if (opt == "EX")
        ttl_ms = amount * 1000;
      else if (opt == "PX")
        ttl_ms = amount;
    } catch (...) {
      return;
    }
  }
  Value v{value, ttl_ms};
  data_store_.set(key, v, ttl_ms);

  RESP response{.resp_type = RESP::type::SIMPLE_STRING, .str = "OK"};
  std::string serialized = serialize_RESP(response);
  robust_send(client_fd, serialized.c_str(), serialized.size());
}

void TCPServer::handle_get(const std::vector<std::string> &tokens,
                           int client_fd) {
  if (tokens.size() < 2)
    return;

  std::string key = tokens[1];
  std::optional<RedisData> opt = data_store_.get(key);
  std::optional<std::string> result;
  if (opt) {
    std::visit(
        [&result](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, std::string>) {
            result = value;
          }
        },
        *opt);
  }

  RESP response;
  if (result) {
    response = {.resp_type = RESP::type::BULK_STRING, .str = *result};
  } else {
    response = {.resp_type = RESP::type::BULK_STRING, .is_null = true};
  }

  std::string serialized = serialize_RESP(response);
  robust_send(client_fd, serialized.c_str(), serialized.size());
}

void TCPServer::handle_rpush(const std::vector<std::string> &tokens,
                             int client_fd) {
  if (tokens.size() < 3) {
    RESP e{.resp_type=RESP::type::ERROR, .str="Wrong number of arguments for RPUSH"};
    std::string serialized = serialize_RESP(e);
    robust_send(client_fd, serialized.c_str(), serialized.size());
    return;
  }

  const std::string& key = tokens[1];
  Value *v = data_store_.get_or_create(key);
  RedisData &d = v->data;

  auto *list = std::get_if<RedisList>(&d);
  if (!list) {
    RESP e{.resp_type=RESP::type::ERROR, .str="KEY HOLDING WRONG TYPE VALUE"};
    std::string serialized = serialize_RESP(e);
    robust_send(client_fd, serialized.c_str(), serialized.size());
    return;
  }

  for (auto i = 2; i < tokens.size(); i++) {
    list->push_back(tokens[i]);
  }

  RESP response {
    .resp_type=RESP::type::INTEGER,
    .integer=static_cast<i64>(list->size()),
  };
  std::string serialized = serialize_RESP(response);
  robust_send(client_fd, serialized.c_str(), serialized.size());
  return;
}

void TCPServer::handle_client(int client_fd, int client_id) {
  std::string buffer;
  while (running_) {
    char temp[4096];
    ssize_t n = recv(client_fd, temp, sizeof(temp), 0);
    if (n <= 0)
      break;

    buffer.append(temp, n);

    while (true) {
      size_t pos = 0;
      try {
        RESP r = parse_RESP(buffer, pos);
        buffer.erase(0, pos);
        if (r.resp_type == RESP::type::ARRAY) {
          std::vector<std::string> tokens = RESP_to_tokens(r);
          execute_command(tokens, client_fd);
        }
      } catch (...) {
        break;
      }
    }
  }
  close(client_fd);
}

void TCPServer::start() {
  std::cout << std::unitbuf;
  running_ = true;

  std::thread maintenance_thread([this]() {
    while (running_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      data_store_.active_expiry_cycle();
    }
  });

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    std::cerr << "Bind failed\n";
    return;
  }

  listen(server_fd, 10);
  std::cout << std::format("Server started on port {}\n", port_);

  std::thread([this, server_fd]() { accept_clients(server_fd); }).detach();

  while (running_) {
    std::string input;
    if (!std::getline(std::cin, input) || input == "exit") {
      stop();
    }
  }

  if (maintenance_thread.joinable()) {
    maintenance_thread.join();
  }
  close(server_fd);
}

void TCPServer::stop() { running_ = false; }

void TCPServer::accept_clients(int server_fd) {
  while (running_) {
    sockaddr_in client{};
    socklen_t addrlen = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &addrlen);
    if (client_fd < 0)
      continue;

    int client_id = ++client_id_counter;
    std::thread([this, client_fd, client_id]() {
      handle_client(client_fd, client_id);
    }).detach();
  }
}

} // namespace Redis