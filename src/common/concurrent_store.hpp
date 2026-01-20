#include "common/types.hpp"
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace Redis {
class ConcurrentStore {
private:
  std::unordered_map<std::string, Value> store_;
  std::unordered_map<std::string, i64> expiry_index_;
  mutable std::shared_mutex mtx_;

public:
  void set(const std::string &key, Value v, i64 ttl_ms = -1);
  std::optional<RedisData> get(const std::string &key);
  Value *get_or_create(const std::string &key);

  void active_expiry_cycle();
};

} // namespace Redis