#pragma once

#include <linux/futex.h>
#include <spdlog/spdlog.h>
#include <syscall.h>
#include <unistd.h>

#include "Futex.h"

namespace libfutex {

class RobustSpinlock {
  static constexpr uint32_t UNLOCKED = FUTEX_OWNER_DIED;
  Futex futex{UNLOCKED};

 public:
  RobustSpinlock() = default;
  RobustSpinlock(const RobustSpinlock&) = delete;
  RobustSpinlock(RobustSpinlock&&) = delete;
  RobustSpinlock& operator=(const RobustSpinlock&) = delete;
  RobustSpinlock& operator=(RobustSpinlock&&) = delete;

  void lock() { futex.lock(lock_impl); }
  void unlock() { futex.unlock(unlock_impl); }

 private:
  static void lock_impl(std::atomic<uint32_t>& val) {
    const void* addr = reinterpret_cast<std::byte*>(&val) - FUTEX_VALUE_OFFSET;
    uint32_t expected = UNLOCKED;
    while (!val.compare_exchange_strong(expected, tid)) {
      expected = UNLOCKED;
    }
    SPDLOG_INFO("acquired spinlock {}", addr);
  }

  static void unlock_impl(std::atomic<uint32_t>& val) {
    const void* addr = reinterpret_cast<std::byte*>(&val) - FUTEX_VALUE_OFFSET;
    uint32_t expected = tid;
    if (!val.compare_exchange_strong(expected, UNLOCKED)) {
      SPDLOG_WARN("released unlocked spinlock {}", addr);
    } else {
      SPDLOG_INFO("released spinlock {}", addr);
    }
  }
};
}  // namespace libfutex
