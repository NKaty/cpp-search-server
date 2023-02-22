#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

template<typename Key, typename Value>
class ConcurrentMap {
 public:
  static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

  struct Access {
    std::lock_guard<std::mutex> guard;
    Value &ref_to_value;
  };

  explicit ConcurrentMap(size_t bucket_count)
      : bucket_count_(bucket_count), buckets_(bucket_count) {};

  Access operator[](const Key &key) {
    auto &bucket = GetBucket(key);
    return {std::lock_guard(bucket.mutex_), bucket.values_[key]};
  };

  std::map<Key, Value> BuildOrdinaryMap() {
    std::map<Key, Value> ordinary_map;
    for (auto &[mutex_, values_] : buckets_) {
      std::lock_guard guard(mutex_);
      ordinary_map.merge(values_);
    }
    return ordinary_map;
  };

  void erase(const Key &key) {
    auto &bucket = GetBucket(key);
    std::lock_guard guard(bucket.mutex_);
    bucket.values_.erase(key);
  }

 private:
  struct Bucket {
    std::mutex mutex_;
    std::map<Key, Value> values_;
  };

  size_t bucket_count_;
  std::vector<Bucket> buckets_;

  Bucket &GetBucket(const Key &key) {
    return buckets_[static_cast<uint64_t>(key) % bucket_count_];
  }
};
