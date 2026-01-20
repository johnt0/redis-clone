#pragma once
#include <string>
#include <vector>


class TCPClient {
public:
  TCPClient();
  void connect_to_server(const std::string &address, int port); 
  void read_from_server(int sock_fd);
private:
  int running_;
  int sock_fd;
};