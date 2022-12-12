#pragma once

#include <linux/futex.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <iostream>

namespace libfutex {
// forward declaration
class Futex;
std::ostream& operator<<(std::ostream& os, const Futex& f);

static constexpr int FUTEX_OFFSET = 16;

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
      .futex_offset = FUTEX_OFFSET,

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
  RobustList() {
    int rc = (int)syscall(SYS_set_robust_list, &head.list, sizeof(head));
    if (rc == 0) {
      SPDLOG_DEBUG("set_robust_list({})", (void*)&head.list);
    } else {
      SPDLOG_ERROR("set_robust_list failed: {}", strerror(rc));
    }
  }

 public:
  [[nodiscard]] size_t size() const {
    size_t size = 0;
    void* ftx = (void*)head.list.next;
    while (ftx != (void*)&head.list) {
      size++;
      ftx = *(void**)ftx;
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

}  // namespace libfutex
