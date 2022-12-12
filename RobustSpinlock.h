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
  [[nodiscard]] bool is_locked() const { return futex.get_val() != UNLOCKED; }

 private:
  static void lock_impl(std::atomic<uint32_t>& val) {
    uint32_t expected = UNLOCKED;
    while (!val.compare_exchange_strong(expected, tid)) {
      expected = UNLOCKED;
    }
    SPDLOG_DEBUG("acquired spinlock {}", fmt::ptr(&val));
  }

  static void unlock_impl(std::atomic<uint32_t>& val) {
    uint32_t expected = tid;
    if (!val.compare_exchange_strong(expected, UNLOCKED)) {
      SPDLOG_WARN("released unlocked spinlock {}", fmt::ptr(&val));
    } else {
      SPDLOG_DEBUG("released spinlock {}", fmt::ptr(&val));
    }
  }
};
}  // namespace libfutex
