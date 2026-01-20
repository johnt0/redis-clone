#pragma once
#include "common/concurrent_store.hpp"
#include <atomic>
#include <string>
#include <vector>

namespace Redis {

class TCPServer {
public:
  TCPServer(const std::string &address, int port);
  ~TCPServer();

  void start();
  void stop();

private:
  void accept_clients(int server_fd);
  void handle_client(int client_fd, int client_id);

  void execute_command(const std::vector<std::string> &tokens, int client_fd);

  void handle_ping(int client_fd);
  void handle_echo(const std::vector<std::string> &tokens, int client_fd);
  void handle_set(const std::vector<std::string> &tokens, int client_fd);
  void handle_get(const std::vector<std::string> &tokens, int client_fd);
  void handle_rpush(const std::vector<std::string> &tokens, int client_fd);

  std::string host_;
  int port_;
  std::atomic<bool> running_;

  ConcurrentStore data_store_;
};

} // namespace Redis


