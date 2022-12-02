#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>

#include "fork.h"
#include "shm.h"

class RobustFutex {
  /**
   * Pointer to the next futex in the list of futexes owned by the same thread.
   * Must be the first field in the class.
   */
  std::atomic<RobustFutex*> next;

  /**
   * Pointer to the previous futex in the list of futexes owned by the same
   * thread. Used to remove the futex from the list when it is destroyed.
   */
  std::atomic<RobustFutex*> prev;

  /**
   * Futex value.
   * Reference:
   *    https://docs.kernel.org/locking/robust-futex-ABI.html
   *    https://www.kernel.org/doc/Documentation/robust-futexes.txt
   */
  std::atomic<uint32_t> val;

 public:
  /**
   * A thread-local list of futexes owned by the thread.
   */
  struct RobustList {
    /**
     * Per-thread list head
     */
    robust_list_head head = {
        /**
         * The head of the list. Points back to itself if empty.
         */
        .list = {.next = &head.list},

        /**
         * Relative offset to the futex value.
         */
        .futex_offset = offsetof(RobustFutex, val),

        /**
         * The address of the to-be-taken lock used to avoid race condition.
         */
        .list_op_pending = nullptr,
    };

    /**
     * Call `set_robust_list` to register the robust list to the kernel, so the
     * kernel can clean up the futexes owned by the thread when the thread
     * exits.
     */
    RobustList() { syscall(SYS_set_robust_list, &head.list, sizeof(head)); }

    friend std::ostream& operator<<(std::ostream& os, const RobustList& rl) {
      os << "RobustList (" << &rl << ") {\n";
      os << "\thead: " << rl.head.list.next << "\n";
      for (auto* p = rl.head.list.next; p != &rl.head.list; p = p->next) {
        os << "\t" << p << ": " << *reinterpret_cast<RobustFutex*>(p) << "\n";
      }
      return os << "}";
    }
  };

  /**
   * A thread-local RobustList instance. The constructor is called when the
   * thread starts to register the list to the kernel.
   */
  inline static thread_local RobustList rlist;

  void lock() {
    // Set pending futex to the current one so that the kernel knows this futex
    // might be locked by the thread.
    rlist.head.list_op_pending = (robust_list*)this;

    lock_impl();

    // Add the current futex to the robust list.
    this->prev = (RobustFutex*)&rlist.head.list;
    this->next = (RobustFutex*)rlist.head.list.next;
    auto old = rlist.head.list.next;
    rlist.head.list.next = (robust_list*)this;
    if (old != &rlist.head.list) ((RobustFutex*)old)->prev = this;

    // Reset pending futex to nullptr when the lock is acquired.
    rlist.head.list_op_pending = nullptr;
  }

  void unlock() {
    unlock_impl();

    // Remove the current futex from the robust list.
    this->next.load()->next = this->prev.load();
    this->prev.load()->next = this->next.load();
  }

 private:
  void lock_impl() {
    uint32_t tid = syscall(SYS_gettid);
    do {
      uint32_t expected = 0;

      // If the futex is not locked (val == 0), try to lock it by setting val to
      // the thread id.
      if (val.compare_exchange_strong(expected, tid)) {
        SPDLOG_INFO("{} acquired unlocked futex", tid);
        return;
      }

      // The value is FUTEX_OWNER_DIED, indicating that the previous owner of
      // the futex died without unlocking it. Try to lock it by setting val to
      // the thread id while keeping the FUTEX_WAITERS bit if it is set.
      if (expected & FUTEX_OWNER_DIED) {
        uint32_t new_val = tid | (expected & FUTEX_WAITERS);
        if (val.compare_exchange_strong(expected, new_val)) {
          SPDLOG_INFO("{} acquired futex w/ owner died", tid);
          return;
        }
      }

      // The futex is locked by another thread. Set the FUTEX_WAITERS bit to let
      // the owner know that there is a thread waiting for the lock.
      val.fetch_or(FUTEX_WAITERS);
      expected = val.load();
      SPDLOG_INFO("{} waiting for {}", tid, expected & FUTEX_TID_MASK);
      if (syscall(SYS_futex, &val, FUTEX_WAIT, expected) == 0) {
        uint32_t zero = 0;
        if (val.compare_exchange_strong(zero, tid)) {
          SPDLOG_INFO("{} acquired futex after waiting for {}", tid,
                      expected & FUTEX_TID_MASK);
          return;
        }
      } else {
        SPDLOG_WARN("FUTEX_WAIT failed with {}: {}", errno, strerror(errno));
      }
    } while (true);
  }

  void unlock_impl() {
    uint32_t tid = syscall(SYS_gettid);
    uint32_t expected = tid;

    if (val.compare_exchange_strong(expected, 0)) {
      SPDLOG_INFO("{} released futex w/ no waiters", expected);
      return;
    }

    if ((expected & ~FUTEX_WAITERS) != tid) {
      SPDLOG_ERROR("unlocking futex owned by another thread");
      return;
    }

    if (val.compare_exchange_strong(expected, 0)) {
      if (syscall(SYS_futex, &val, FUTEX_WAKE, 1) != 1) {
        SPDLOG_WARN("FUTEX_WAKE failed with {}: {}", errno, strerror(errno));
      }
      SPDLOG_INFO("{} released futex and waked up waiter", tid);
    } else {
      SPDLOG_ERROR("unlocking futex owned by another thread");
    }
  }

 public:
  friend std::ostream& operator<<(std::ostream& os, const RobustFutex& f) {
    return os << "Futex{val = " << f.val
              << " (tid = " << (f.val & FUTEX_TID_MASK)
              << "), prev = " << f.prev << ", next = " << f.next << "}";
  }
};

void test_robust() {
  SharedMemory<RobustFutex> ftx1;
  SharedMemory<RobustFutex> ftx2;

  fork_and_wait([&]() {
    std::cout << RobustFutex::rlist << std::endl;
    ftx1->lock();
    ftx2->lock();
    std::cout << RobustFutex::rlist << std::endl;
  });

  ftx1->lock();
  ftx2->lock();
  std::cout << RobustFutex::rlist << std::endl;

  ftx1->unlock();
  std::cout << RobustFutex::rlist << std::endl;
  ftx2->unlock();
  std::cout << RobustFutex::rlist << std::endl;

  ftx1->lock();
  ftx2->lock();
  std::cout << RobustFutex::rlist << std::endl;
}

void test_sync() {
  SharedMemory<RobustFutex> ftx;

  std::thread t1([&]() {
    ftx->lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ftx->unlock();
  });

  std::thread t2([&]() {
    ftx->lock();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ftx->unlock();
  });

  t1.join();
  t2.join();
}

int main() {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%P-%t] %v");
  spdlog::set_level(spdlog::level::debug);
  test_robust();
  test_sync();
}
