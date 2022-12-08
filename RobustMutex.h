#pragma once

#include <linux/futex.h>
#include <spdlog/spdlog.h>
#include <syscall.h>
#include <unistd.h>

#include "Futex.h"

namespace libfutex {
class RobustMutex {
  Futex futex;

 public:
  RobustMutex() = default;
  RobustMutex(const RobustMutex&) = delete;
  RobustMutex(RobustMutex&&) = delete;
  RobustMutex& operator=(const RobustMutex&) = delete;
  RobustMutex& operator=(RobustMutex&&) = delete;

  void lock() { futex.lock(lock_impl); }
  void unlock() { futex.unlock(unlock_impl); }

 private:
  static void lock_impl(std::atomic<uint32_t>& val) {
    const void* addr = reinterpret_cast<std::byte*>(&val) - FUTEX_VALUE_OFFSET;
    do {
      uint32_t expected = 0;

      // If the futex is not locked (val == 0), try to lock it by setting val
      // to the thread id.
      if (val.compare_exchange_strong(expected, tid)) {
        SPDLOG_INFO("acquired unlocked futex {}", addr);
        return;
      }

      // The value is FUTEX_OWNER_DIED, indicating that the previous owner of
      // the futex died without unlocking it. Try to lock it by setting val to
      // the thread id while keeping the FUTEX_WAITERS bit if it is set.
      if (expected & FUTEX_OWNER_DIED) {
        uint32_t new_val = tid | (expected & FUTEX_WAITERS);
        if (val.compare_exchange_strong(expected, new_val)) {
          SPDLOG_INFO("acquired futex {} w/ owner died", addr);
          return;
        }
      }

      // The futex is locked by another thread. Set the FUTEX_WAITERS bit to
      // let the owner know that there is a thread waiting for the lock.
      val.fetch_or(FUTEX_WAITERS);
      expected = val.load();
      SPDLOG_INFO("waiting for {}", expected & FUTEX_TID_MASK);
      if (syscall(SYS_futex, &val, FUTEX_WAIT, expected) == 0) {
        uint32_t zero = 0;
        if (val.compare_exchange_strong(zero, tid)) {
          SPDLOG_INFO("acquired futex {} after waiting for {}", addr,
                      expected & FUTEX_TID_MASK);
          return;
        }
      } else {
        SPDLOG_WARN("FUTEX_WAIT {} failed with {}: {}", addr, errno,
                    strerror(errno));
      }
    } while (true);
  }

  static void unlock_impl(std::atomic<uint32_t>& val) {
    const void* addr = reinterpret_cast<std::byte*>(&val) - FUTEX_VALUE_OFFSET;
    uint32_t expected = tid;

    if (val.compare_exchange_strong(expected, 0)) {
      SPDLOG_INFO("released futex {} w/ no waiters", addr);
      return;
    }

    if ((expected & ~FUTEX_WAITERS) != tid) {
      SPDLOG_ERROR("unlocking futex {} owned by another thread", addr);
      return;
    }

    if (val.compare_exchange_strong(expected, 0)) {
      if (syscall(SYS_futex, &val, FUTEX_WAKE, 1) != 1) {
        SPDLOG_WARN("FUTEX_WAKE {} failed with {}: {}", addr, errno,
                    strerror(errno));
      }
      SPDLOG_INFO("released futex {} and waked up waiter", addr);
    } else {
      SPDLOG_ERROR("unlocking futex {} owned by another thread", addr);
    }
  }
};
}  // namespace libfutex
