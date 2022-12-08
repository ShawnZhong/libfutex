
#include <spdlog/spdlog.h>

#include "RobustMutex.h"
#include "RobustSpinlock.h"

using namespace libfutex;

template <typename T>
void test_robust() {
  T ftx1;
  T ftx2;

  std::jthread t1([&]() {
    Futex::rlist.print();
    ftx1.lock();
    ftx2.lock();
    Futex::rlist.print();
  });

  ftx1.lock();
  ftx2.lock();
  Futex::rlist.print();

  ftx1.unlock();
  Futex::rlist.print();
  ftx2.unlock();
  Futex::rlist.print();

  ftx1.lock();
  ftx2.lock();
  Futex::rlist.print();
}

template <typename T>
void test_sync() {
  T ftx;

  ftx.lock();

  std::jthread t1([&]() { ftx.lock(); });

  std::this_thread::sleep_for(std::chrono::seconds(3));
  ftx.unlock();
}

int main() {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");
  test_robust<RobustSpinlock>();
  test_sync<RobustSpinlock>();
  test_robust<RobustMutex>();
  test_sync<RobustMutex>();
}
