#include "server/tcp_server.hpp"
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
  int port = 6379;
  if (argc > 1)
    port = std::stoi(argv[1]);

  Redis::TCPServer server("0.0.0.0", port);
  std::thread t([&]() { server.start(); });

  std::cout << "Redis clone running on port " << port << "...\n";
  t.join();
  return 0;
}