#include "RESP.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
// #include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <vector>

/*
struct RESP {
  enum class type { SIMPLE_STRING, ERROR, INTEGER, BULK_STRING, ARRAY };
  type resp_type;
  std::string str;
  long long integer = 0;
  std::vector<RESP> elements;
  bool is_null = false;
};
*/

// convert a vector into a string
std::string vector_to_str(const std::vector<std::string> &vec) {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    oss << "\"" << vec[i] << "\"";
    if (i != vec.size() - 1) {
      oss << ", ";
    }
  }
  return oss.str();
}

// reads line of string until \r\n is no longer found
static std::string read_line_CRLF(const std::string &data, size_t &pos) {
  size_t e = data.find("\r\n", pos);
  if (e == std::string::npos) {
    throw std::runtime_error("CRLF not found");
  }
  std::string out = data.substr(pos, e - pos);
  pos = e + 2;
  return out;
}
// attempts to convert to long long
static long long stoll_safe(const std::string &line) {
  try {
    size_t pos;
    long long val = std::stoll(line, &pos);

    if (pos != line.length()) {
      throw std::runtime_error("Invalid characters in number string");
    }
    return val;
  } catch (const std::invalid_argument &) {
    throw std::runtime_error("Invalid number format");
  } catch (const std::out_of_range &) {
    throw std::runtime_error("Number out of range");
  }
}

// converts a string into a RESP structure
RESP convert_inline_to_RESP(const std::string &input) {
  auto words = split_lines(input);
  RESP resp{};
  resp.resp_type = RESP::type::ARRAY;
  resp.elements.reserve(words.size() + 1);
  for (const auto &word : words) {
    RESP r{};
    r.resp_type = RESP::type::BULK_STRING;
    r.str = word;
    resp.elements.emplace_back(r);
  }
  return resp;
}
// returns a vector of strings split on spaces
static std::vector<std::string> split_lines(const std::string &data) {
  std::vector<std::string> lines;
  size_t start = 0;
  size_t end = data.find(" ");
  while (end != std::string::npos) {
    lines.push_back(data.substr(start, end - start));
    start = end + 1;
    end = data.find(" ", start);
  }
  if (start < data.size()) {
    lines.push_back(data.substr(start));
  }
  return lines;
}

// convert client/server sent RESP protocol string into a RESP structure
RESP parse_RESP(const std::string &data, size_t &pos) {
  return parse_RESP_internal(data, pos, 0);
}

static RESP parse_RESP_internal(const std::string &data, size_t &pos,
                                int depth) {
  if (depth > MAX_RESP_DEPTH) {
    throw std::runtime_error("RESP nesting depth exceeded");
  }

  if (pos >= data.size()) {
    throw std::runtime_error("Unexpected end of data");
  }

  char prefix = data[pos++];
  std::string line;

  switch (prefix) {
  case '+':
  case '-': {
    RESP resp{};
    resp.resp_type =
        (prefix == '+') ? RESP::type::SIMPLE_STRING : RESP::type::ERROR;
    resp.str = read_line_CRLF(data, pos);
    return resp;
  }
  case ':': {
    RESP resp{};
    resp.resp_type = RESP::type::INTEGER;
    line = read_line_CRLF(data, pos);
    resp.integer = stoll_safe(line);
    return resp;
  }
  case '$': {
    line = read_line_CRLF(data, pos);
    long long len = stoll_safe(line);

    if (len == -1) {
      RESP resp{};
      resp.resp_type = RESP::type::BULK_STRING;
      resp.is_null = true;
      return resp;
    }

    if (len < 0 || len > MAX_BULK_LEN) {
      throw std::runtime_error("Invalid bulk string length");
    }

    size_t safe_len = static_cast<size_t>(len);

    if (pos + safe_len + 2 > data.size()) {
      throw std::runtime_error("Bulk string data boundary error");
    }

    std::string bulk_data = data.substr(pos, safe_len);

    if (data.substr(pos + safe_len, 2) != "\r\n") {
      throw std::runtime_error("Bulk string missing trailing CRLF");
    }

    RESP resp{};
    resp.resp_type = RESP::type::BULK_STRING;
    resp.str = bulk_data;
    pos += safe_len + 2;
    return resp;
  }
  case '*': {
    line = read_line_CRLF(data, pos);
    long long count = stoll_safe(line);

    if (count == -1) {
      RESP resp{};
      resp.resp_type = RESP::type::ARRAY;
      resp.is_null = true;
      return resp;
    }

    if (count < 0 || count > MAX_ARRAY_COUNT) {
      throw std::runtime_error("Invalid array count");
    }

    RESP resp{};
    resp.resp_type = RESP::type::ARRAY;
    resp.elements.reserve(static_cast<size_t>(count));
    for (long long i = 0; i < count; ++i) {
      resp.elements.emplace_back(parse_RESP_internal(data, pos, depth + 1));
    }
    return resp;
  }
  default:
    throw std::runtime_error("Unsupported RESP type");
  }
}
// convert a RESP struct into a string
std::string serialize_RESP(const RESP &resp) {
  switch (resp.resp_type) {
  case RESP::type::SIMPLE_STRING:
    return "+" + resp.str + "\r\n";
  case RESP::type::ERROR:
    return "-" + resp.str + "\r\n";
  case RESP::type::INTEGER:
    return ":" + std::to_string(resp.integer) + "\r\n";
  case RESP::type::BULK_STRING:
    if (resp.is_null) {
      return "$-1\r\n";
    } else {
      return "$" + std::to_string(resp.str.size()) + "\r\n" + resp.str + "\r\n";
    }
  case RESP::type::ARRAY:
    if (resp.is_null) {
      return "*-1\r\n";
    } else {
      std::stringstream ss;
      ss << "*" << resp.elements.size() << "\r\n";
      for (const auto &element : resp.elements) {
        ss << serialize_RESP(element);
      }
      return ss.str();
    }
    break;
  default:
    throw std::runtime_error("Unsupported RESP type for serialization");
  }
}

bool robust_send(int sock_fd, const char *data, size_t len) {
  if (sock_fd < 0 || !data || len == 0) {
    return false;
  }

  size_t sent = 0;

  while (sent < len) {
    ssize_t n = send(sock_fd, data + sent, len - sent, MSG_NOSIGNAL);

    if (n < 0) {

      if (errno == EINTR) {
        // Signal interrupted system call - retry without counting as sent
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Socket buffer full - would block
        // In a real app, use select/epoll to wait for socket to be writable
        // For now, return failure (caller can retry or use async I/O)
        return false;
      }
      // Connection errors: ECONNRESET, EPIPE, EBADF, etc.
      return false;
    }

    if (n == 0) {
      // send() returned 0, which shouldn't happen but indicates failure
      return false;
    }
    sent += static_cast<size_t>(n);
  }

  return true;
}

// tokenize a RESP struct into a vector
std::vector<std::string> RESP_to_tokens(const RESP &resp) {
  std::vector<std::string> tokens;

  if (resp.resp_type != RESP::type::ARRAY) {
    throw std::runtime_error("RESP_to_tokens expects an ARRAY type RESP");
  }

  for (const auto &element : resp.elements) {
    if (element.resp_type != RESP::type::BULK_STRING) {
      throw std::runtime_error(
          "RESP_to_tokens expects BULK_STRING elements in the ARRAY");
    }
    tokens.push_back(element.str);
  }

  return tokens;
}

void print_RESP(const RESP &resp) {
  switch (resp.resp_type) {
  case RESP::type::SIMPLE_STRING:
  case RESP::type::BULK_STRING:
    std::cout << resp.str << '\n';
    break;
  case RESP::type::INTEGER:
    std::cout << resp.integer << '\n';
    break;
  case RESP::type::ARRAY:
    for (auto ele : resp.elements) {
      print_RESP(ele);
    }
    break;
  default:
    throw std::runtime_error("unknown RESP type");
  }
}