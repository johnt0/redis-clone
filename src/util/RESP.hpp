#pragma once
// #include <limits>
#include <netinet/in.h>
#include <netinet/tcp.h>
// #include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <vector>

struct RESP {
  enum class type { SIMPLE_STRING, ERROR, INTEGER, BULK_STRING, ARRAY };
  type resp_type;
  std::string str;
  long long integer = 0;
  std::vector<RESP> elements;
  bool is_null = false;
};

constexpr int MAX_RESP_DEPTH = 128;
constexpr long long MAX_BULK_LEN = 512 * 1024 * 1024;
constexpr long long MAX_ARRAY_COUNT = 1024 * 1024;

static std::string read_line_CRLF(const std::string &data, size_t &pos);
static long long stoll_safe(const std::string &line);
static RESP parse_RESP_internal(const std::string &data, size_t &pos,
                                int depth);
static std::vector<std::string> split_lines(const std::string &data);
RESP convert_inline_to_RESP(const std::string &input);
RESP parse_RESP(const std::string &data, size_t &pos);
std::string serialize_RESP(const RESP &resp);
void send_RESP(int sock_fd, const RESP &resp);
bool robust_send(int sock_fd, const char *data, size_t len);
std::vector<std::string> RESP_to_tokens(const RESP &resp);
void print_RESP(const RESP &resp);
void read();

inline void configure_socket_safety(int sock_fd) {
  int nodelay = 1;
  setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
  int keepalive = 1;
  setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
}