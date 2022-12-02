#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/user.h>

#include <stdexcept>
#include <string>

template <typename T>
struct SharedMemory {
  T *const buf;

  template <typename... Args>
  explicit SharedMemory(Args &&...args)
      : buf((T *)mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0)) {
    if (buf == MAP_FAILED) throw std::runtime_error("mmap failed");
    new (buf) T(std::forward<Args>(args)...);
  }
  SharedMemory(const SharedMemory &) = delete;
  SharedMemory(SharedMemory &&) = delete;
  SharedMemory &operator=(const SharedMemory &) = delete;
  SharedMemory &operator=(SharedMemory &&) = delete;
  ~SharedMemory() {
    buf->~T();
    munmap(buf, sizeof(T));
  }
  T *operator->() const { return buf; }
  T &operator*() const { return *buf; }
  operator T *() const { return buf; }
};
