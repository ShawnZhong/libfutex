
#include <spdlog/spdlog.h>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "RobustMutex.h"
#include "RobustSpinlock.h"

using namespace libfutex;

template <typename T>
void test_robust() {
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

  std::thread thread([&] {
    check_both_unlocked();
    lock_both();
    check_both_locked();
    // We don't unlock the futexes here.
  });
  thread.join();

  // The kernel should have unlocked the futexes left by the thread.
  check_both_unlocked();

  lock_both();
  check_both_locked();

  unlock_both();
  check_both_unlocked();

  lock_both();
  check_both_locked();
}

template <typename T>
void test_sync() {
  T ftx;

  ftx.lock();

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::jthread t1([&]() { ftx.lock(); });

  std::this_thread::sleep_for(std::chrono::seconds(1));
  ftx.unlock();
}

TEST_CASE("RobustSpinlock", "[RobustSpinlock]") {
  SECTION("robust") { test_robust<RobustSpinlock>(); }
  SECTION("sync") { test_sync<RobustSpinlock>(); }
}

int main(int argc, char* argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");
  return Catch::Session().run(argc, argv);
}
