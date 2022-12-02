#include "robust_mutex.h"

#include <spdlog/spdlog.h>

#include "fork.h"
#include "shm.h"

void test_thread() {
  SPDLOG_INFO("test_thread");
  RobustMutex mutex;
  std::thread([&]() { mutex.lock(); }).join();
  mutex.lock();
  mutex.unlock();
}

void test_proc() {
  SPDLOG_INFO("test_proc");
  SharedMemory<RobustMutex> mutex;
  fork_and_wait([&]() { mutex->lock(); });
  mutex->lock();
  mutex->unlock();
}

int main() {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P-%t] [%^%l%$] [%s:%#] %v");
  test_thread();
  test_proc();
}
