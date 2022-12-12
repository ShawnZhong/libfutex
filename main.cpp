#ifndef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <cxxabi.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "RobustMutex.h"
#include "RobustSpinlock.h"

using namespace libfutex;

template <typename T>
std::string type_name() {
  const char* mangled = typeid(T).name();
  int status;
  char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  if (status == 0) {
    std::string result(demangled);
    std::free(demangled);
    return result;
  } else {
    return mangled;
  }
}

template <typename T>
void test_sync() {
  SPDLOG_INFO("Testing synchronization of {}", type_name<T>());
  using namespace std::chrono_literals;
  using time_point = std::chrono::high_resolution_clock::time_point;
  using clock = std::chrono::high_resolution_clock;

  time_point t1_locked_ts;    // When T1 first gets the futex.
  time_point t2_start_ts;     // When T2 starts waiting for the futex.
  time_point t1_unlocked_ts;  // When T1 releases the futex.
  time_point t2_end_ts;       // When T2 gets the futex.

  {
    REQUIRE(Futex::rlist.size() == 0);
    T ftx;
    ftx.lock();
    t1_locked_ts = clock::now();
    std::thread t2([&]() {
      t2_start_ts = clock::now();
      ftx.lock();
      t2_end_ts = clock::now();
    });
    std::this_thread::sleep_for(1s);
    ftx.unlock();
    t1_unlocked_ts = clock::now();
    t2.join();
  }

  REQUIRE(t2_start_ts - t1_locked_ts < 1ms);  // T2 starts waiting immediately.
  REQUIRE(t2_end_ts - t1_unlocked_ts < 1ms);  // T2 gets the futex immediately.
  REQUIRE(t2_end_ts - t2_start_ts > 0.9s);    // T2 waits for 1s.
  REQUIRE(t2_end_ts - t2_start_ts < 1.1s);
}

template <typename T>
void test_robust() {
  SPDLOG_INFO("Testing robustness of {}", type_name<T>());
  T ftx1;
  T ftx2;

  auto check_both_unlocked = [&] {
    REQUIRE(!ftx1.is_locked());
    REQUIRE(!ftx2.is_locked());
    REQUIRE(Futex::rlist.size() == 0);
  };

  auto check_both_locked = [&] {
    REQUIRE(ftx1.is_locked());
    REQUIRE(ftx2.is_locked());
    REQUIRE(Futex::rlist.size() == 2);
  };

  auto lock_both = [&] {
    ftx1.lock();
    REQUIRE(ftx1.is_locked());
    REQUIRE(Futex::rlist.size() == 1);
    ftx2.lock();
    REQUIRE(ftx2.is_locked());
    REQUIRE(Futex::rlist.size() == 2);
  };

  auto unlock_both = [&] {
    ftx1.unlock();
    REQUIRE(!ftx1.is_locked());
    REQUIRE(Futex::rlist.size() == 1);
    ftx2.unlock();
    REQUIRE(!ftx2.is_locked());
    REQUIRE(Futex::rlist.size() == 0);
  };

  check_both_unlocked();

  std::thread([&] {
    check_both_unlocked();
    lock_both();
    check_both_locked();
    // We don't unlock the futexes here.
  }).join();

  // The kernel should have unlocked the futexes left by the thread.
  check_both_unlocked();

  lock_both();
  check_both_locked();

  unlock_both();
  check_both_unlocked();
}

TEST_CASE("RobustSpinlock::sync") { test_sync<RobustSpinlock>(); }
TEST_CASE("RobustSpinlock::robust") { test_robust<RobustSpinlock>(); }
TEST_CASE("RobustMutex::sync") { test_sync<RobustMutex>(); }
TEST_CASE("RobustMutex::robust") { test_robust<RobustMutex>(); }

int main(int argc, char* argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");
  spdlog::cfg::load_env_levels();
  return Catch::Session().run(argc, argv);
}
