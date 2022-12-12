#pragma once

#include <linux/futex.h>
#include <spdlog/fmt/bundled/ostream.h>
#include <spdlog/spdlog.h>
#include <syscall.h>
#include <unistd.h>

#include <atomic>

namespace libfutex {

inline static thread_local const uint32_t tid = (uint32_t)syscall(SYS_gettid);

class Futex {
  /**
   * Pointer to the next futex in the list of futexes owned by the same thread.
   * Must be the first field in the class.
   */
  std::atomic<Futex*> next;

  /**
   * Pointer to the previous futex in the list of futexes owned by the same
   * thread. Used to remove the futex from the list when it is destroyed.
   */
  std::atomic<Futex*> prev;

  /**
   * Futex value.
   * Reference:
   *    https://docs.kernel.org/locking/robust-futexes.html
   *    https://docs.kernel.org/locking/robust-futex-ABI.html
   */
  std::atomic<uint32_t> val;

 public:
  Futex() = default;
  explicit Futex(uint32_t val) : val(val) {}

  template <typename Fn>
  void lock(Fn&& fn) {
    // Set pending futex to the current one so that the kernel knows this futex
    // might be locked by the thread.
    rlist.head.list_op_pending = (robust_list*)this;

    fn(val);

    // Add the current futex to the robust list.
    this->prev = (Futex*)&rlist.head.list;
    this->next = (Futex*)rlist.head.list.next;
    auto old = rlist.head.list.next;
    rlist.head.list.next = (robust_list*)this;
    if (old != &rlist.head.list) ((Futex*)old)->prev = this;

    // Reset pending futex to nullptr when the lock is acquired.
    rlist.head.list_op_pending = nullptr;
  }

  template <typename Fn>
  void unlock(Fn&& fn) {
    fn(val);

    // Remove the current futex from the robust list.
    this->next.load()->next = this->prev.load();
    this->prev.load()->next = this->next.load();
  }

  [[nodiscard]] uint32_t get_val() const { return val.load(); }

  /**
   * A thread-local list of futexes owned by the thread.
   */
  class RobustList {
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
        .futex_offset = offsetof(Futex, val),

        /**
         * The address of the to-be-taken lock used to avoid race condition.
         */
        .list_op_pending = nullptr,
    };

   public:
    /**
     * Call `set_robust_list` to register the robust list to the kernel, so the
     * kernel can clean up the futexes owned by the thread when the thread
     * exits.
     */
    RobustList() {
      int rc = (int) syscall(SYS_set_robust_list, &head.list, sizeof(head));
      if (rc == 0) {
        SPDLOG_DEBUG("set_robust_list({})", (void*)&head.list);
      } else {
        SPDLOG_ERROR("set_robust_list failed: {}", strerror(rc));
      }
    }

    void print() const { SPDLOG_INFO("{}", fmt::streamed(*this)); }

    [[nodiscard]] size_t size() const {
      size_t size = 0;
      auto* ftx = (Futex*)head.list.next;
      while (ftx != (Futex*)&head.list) {
        size++;
        ftx = ftx->next;
      }
      return size;
    }

    friend std::ostream& operator<<(std::ostream& os, const RobustList& rl) {
      os << "RobustList (" << &rl << ") ";
      if (rl.head.list.next == &rl.head.list) {
        os << "{}";
      } else {
        os << "{\n";
        for (auto* p = rl.head.list.next; p != &rl.head.list; p = p->next) {
          os << "\t" << p << ": " << *reinterpret_cast<Futex*>(p) << ", \n";
        }
        os << "}";
      }
      return os;
    }
    friend class Futex;
  };

  /**
   * A thread-local RobustList instance. The constructor is called when the
   * thread starts to register the list to the kernel.
   */
  inline static thread_local RobustList rlist;

  friend std::ostream& operator<<(std::ostream& os, const Futex& f) {
    uint32_t v = f.val.load();
    void* a = (void*)&f.val;
    void* p = f.prev.load();
    void* n = f.next.load();

    if (v & ~FUTEX_TID_MASK) {
      os << fmt::format(
          "Futex{{val = {} | {:#x}, &val = {}, prev = {}, next = {}}}",
          v & FUTEX_TID_MASK, v & ~FUTEX_TID_MASK, a, p, n);
    } else {
      os << fmt::format("Futex{{val = {}, &val = {}, prev = {}, next = {}}}", v,
                        a, p, n);
    }
    return os;
  }
};

}  // namespace libfutex
