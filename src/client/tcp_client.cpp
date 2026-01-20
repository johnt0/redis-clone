#include "../client/tcp_client.hpp"
#include "util/RESP.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

std::atomic<bool> running_{true};
std::mutex cout_mutex;

TCPClient::TCPClient() : sock_fd(-1) {}

void TCPClient::connect_to_server(const std::string &address, int port) {
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0)
    throw std::runtime_error("Socket creation failed");

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0)
    throw std::runtime_error("Invalid address/ Address not supported");

  if (connect(sock_fd, reinterpret_cast<sockaddr *>(&server_addr),
              sizeof(server_addr)) < 0)
    throw std::runtime_error("Connection Failed (Client)");

  {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "Connecting to " << address << " on port " << port << "\n";
  }

  std::thread client_loop(&TCPClient::read_from_server, this, sock_fd);
  client_loop.detach();

  while (running_) {
    {
      std::lock_guard<std::mutex> lock(cout_mutex);
      std::cout << "$ redis-cli: " << std::flush;
    }

    std::string input;
    std::getline(std::cin, input);

    if (input == "exit") {
      running_ = false;
      shutdown(sock_fd, SHUT_RDWR);
      break;
    }

    RESP r = convert_inline_to_RESP(input);
    std::string msg = serialize_RESP(r);

    if (!robust_send(sock_fd, msg.c_str(), msg.size()))
      throw std::runtime_error("Failed to send data to server");
  }
}

void TCPClient::read_from_server(int sock_fd) {
  char buffer[4096];
  while (running_) {
    ssize_t bytes_read = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
      running_ = false;
      break;
    }

    buffer[bytes_read] = '\0';
    std::string data(buffer);
    size_t pos = 0;

    try {
      while (pos < data.size()) {
        RESP r = parse_RESP(data, pos);

        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\r\033[K";
        print_RESP(r);
        std::cout << "$ redis-cli: " << std::flush;
      }
    } catch (...) {
      break;
    }
  }
  close(sock_fd);
  running_ = false;
}

int main() {
  TCPClient client;
  int port = 6379;
  std::string address = "0.0.0.0";

  try {
    client.connect_to_server(address, port);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}