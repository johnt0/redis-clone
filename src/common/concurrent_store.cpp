#include "concurrent_store.hpp"
#include "common/types.hpp"
#include <mutex>

namespace Redis {
static i64 get_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void ConcurrentStore::set(const std::string &key, Value v, i64 ttl_ms) {
  std::unique_lock lock(mtx_);
  
  if (ttl_ms > 0) {
    v.expires_at = get_now_ms() + ttl_ms;
    expiry_index_[key] = v.expires_at;
  } else {
    v.expires_at = 0;
    expiry_index_.erase(key);
  }

  store_[key] = std::move(v);
}

std::optional<RedisData> ConcurrentStore::get(const std::string &key) {
  std::shared_lock lock(mtx_);
  auto it = store_.find(key);

  if (it == store_.end()) {
    return std::nullopt;
  }

  if (!it->second.is_persistent() && it->second.expires_at < get_now_ms()) {
    lock.unlock();
    std::unique_lock write_lock(mtx_);

    store_.erase(key);
    expiry_index_.erase(key);
    return std::nullopt;
  }

  return it->second.data;
}

Value* ConcurrentStore::get_or_create(const std::string &key) {
  std::unique_lock lock(mtx_);

  auto it = store_.find(key);
  if (it == store_.end()) {
    auto [new_it, _] = store_.emplace(key, Value{RedisList{}});
    return &new_it->second;
  }
  
  return &it->second;
}

void ConcurrentStore::active_expiry_cycle() {
  std::unique_lock lock(mtx_);
  if (expiry_index_.empty()) {
    return;
  }

  i64 now = get_now_ms();
  int limit = 20;

  auto it = expiry_index_.begin();

  while (it != expiry_index_.end() && limit > 0) {
    if (it->second < now) {
      store_.erase(it->first);
      it = expiry_index_.erase(it);
    } else {
      it++;
    }
    limit--;
  }
}
} // namespace Redis