#pragma once
#include <fcntl.h>
#include <pthread.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
class RobustMutex {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
 public:
  RobustMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mutex, &attr);
  }
  RobustMutex(const RobustMutex&) = delete;
  RobustMutex& operator=(const RobustMutex&) = delete;
  RobustMutex(RobustMutex&&) = delete;
  RobustMutex& operator=(RobustMutex&&) = delete;
  ~RobustMutex() { pthread_mutex_destroy(&mutex); }
  void lock() {
    int rc = pthread_mutex_lock(&mutex);
    if (rc == EOWNERDEAD) {
      SPDLOG_INFO("pthread_mutex_lock returned EOWNERDEAD");
      this->consistent();
    } else if (rc != 0) {
      SPDLOG_ERROR("pthread_mutex_lock failed with error {}", strerror(rc));
    } else {
      SPDLOG_INFO("pthread_mutex_lock succeeded");
    }
  }
  void unlock() {
    int rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
      SPDLOG_ERROR("pthread_mutex_unlock failed: {}", strerror(rc));
    } else {
      SPDLOG_INFO("pthread_mutex_unlock succeeded");
    }
  }
 private:
  void consistent() {
    int rc = pthread_mutex_consistent(&mutex);
    if (rc != 0) {
      SPDLOG_ERROR("pthread_mutex_consistent failed: {}", strerror(rc));
    } else {
      SPDLOG_INFO("pthread_mutex_consistent succeeded");
    }
  }
};
