#include "sylar/log.h"
#include "sylar/sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_rwmutex() {
  sylar::RWMutex rw_mutex;
  int value = 0;
  std::atomic<int> read_sum{0};

  auto reader = [&]() {
    for (int i = 0; i < 10000; ++i) {
      sylar::RWMutex::ReadLock lock(rw_mutex);
      read_sum += value;
    }
  };

  auto writer = [&]() {
    for (int i = 0; i < 10000; ++i) {
      sylar::RWMutex::WriteLock lock(rw_mutex);
      ++value;
    }
  };

  std::vector<std::thread> threads;
  // 3 readers
  for (int i = 0; i < 3; ++i) {
    threads.emplace_back(reader);
  }
  // 2 writers
  for (int i = 0; i < 2; ++i) {
    threads.emplace_back(writer);
  }

  for (auto& t : threads)
    t.join();

  // RWMutex test done, value= 20000 read_sum=xxx
  SYLAR_LOG_INFO(g_logger) << "RWMutex test done, value=" << value
                           << " read_sum=" << read_sum;
}

void test_spinlock() {
  sylar::Spinlock spin;
  int counter = 0;

  auto worker = [&]() {
    for (int i = 0; i < 100000; ++i) {
      sylar::Spinlock::Lock lock(spin);
      ++counter;
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back(worker);
  }
  for (auto& t : threads)
    t.join();

  // Spinlock test done, counter=400000
  SYLAR_LOG_INFO(g_logger) << "Spinlock test done, counter=" << counter;
}

int main(int argc, char** argv) {
  SYLAR_LOG_INFO(g_logger) << "main";
  sylar::Scheduler sc(4, false, "test");
  sc.start();
  sc.schedule(&test_rwmutex);
  sc.schedule(&test_spinlock);
  sc.stop();
  return 0;
}
