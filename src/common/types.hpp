#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <variant>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

namespace Redis {
using RedisList = std::deque<std::string>;
using RedisData = std::variant<std::string, RedisList>;

struct Value {
  RedisData data;
  i64 expires_at;

  explicit Value(RedisData d) : data(std::move(d)), expires_at(0) {}

  Value(RedisData d, i64 exp) : data(std::move(d)), expires_at(exp) {}

  bool is_persistent() const { return expires_at <= 0; }

  bool is_string() const { return std::holds_alternative<std::string>(data); }
  bool is_list() const {
    return std::holds_alternative<std::deque<std::string>>(data);
  }
};
} // namespace Redis
