#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <tbb/spin_rw_mutex.h>
#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <thread>

#include "fiber.h"
#include "noncopyable.h"

namespace sylar {

class Semaphore : Noncopyable {
 public:
  Semaphore(uint32_t count = 0);

  ~Semaphore();

  void wait();

  void notify();

 private:
  sem_t m_semaphore;
};

template <class T>
struct ScopedLockImpl {
 public:
  ScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~ScopedLockImpl() { unlock(); }

  void lock() {
    if (!m_locked) {
      m_mutex.lock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_locked = false;
      m_mutex.unlock();
    }
  }

 private:
  /// mutex
  T& m_mutex;
  /// 是否已上锁
  bool m_locked = false;
};

template <class T>
struct ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~ReadScopedLockImpl() { unlock(); }

  void lock() {
    if (!m_locked) {
      m_locked = true;
      m_mutex.rdlock();
    }
  }

  void unlock() {
    if (m_locked) {
      m_locked = false;
      m_mutex.unlock();
    }
  }

 private:
  /// mutex
  T& m_mutex;
  /// 是否已上锁
  bool m_locked = false;
};

template <class T>
struct WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!m_locked) {
      m_mutex.wrlock();
      m_locked = true;
    }
  }

  void unlock() {
    if (m_locked) {
      m_locked = false;
      m_mutex.unlock();
    }
  }

 private:
  /// Mutex
  T& m_mutex;
  /// 是否已上锁
  bool m_locked = false;
};

class Mutex : Noncopyable {
 public:
  /// 局部锁
  typedef ScopedLockImpl<Mutex> Lock;

  Mutex() { pthread_mutex_init(&m_mutex, nullptr); }

  ~Mutex() { pthread_mutex_destroy(&m_mutex); }

  void lock() { pthread_mutex_lock(&m_mutex); }

  void unlock() { pthread_mutex_unlock(&m_mutex); }

 private:
  /// mutex
  pthread_mutex_t m_mutex;
};

class NullMutex : Noncopyable {
 public:
  /// 局部锁
  typedef ScopedLockImpl<NullMutex> Lock;

  NullMutex() {}

  ~NullMutex() {}

  void lock() {}

  void unlock() {}
};

class RWMutex : Noncopyable {
 public:
  /// 局部读锁
  typedef ReadScopedLockImpl<RWMutex> ReadLock;

  /// 局部写锁
  typedef WriteScopedLockImpl<RWMutex> WriteLock;

  RWMutex() { pthread_rwlock_init(&m_lock, nullptr); }

  ~RWMutex() { pthread_rwlock_destroy(&m_lock); }

  void rdlock() { pthread_rwlock_rdlock(&m_lock); }

  void wrlock() { pthread_rwlock_wrlock(&m_lock); }

  void unlock() { pthread_rwlock_unlock(&m_lock); }

 private:
  /// 读写锁
  pthread_rwlock_t m_lock;
};

class NullRWMutex : Noncopyable {
 public:
  /// 局部读锁
  typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
  /// 局部写锁
  typedef WriteScopedLockImpl<NullRWMutex> WriteLock;

  NullRWMutex() {}

  ~NullRWMutex() {}

  void rdlock() {}

  void wrlock() {}

  void unlock() {}
};

class Spinlock : Noncopyable {
 public:
  /// 局部锁
  typedef ScopedLockImpl<Spinlock> Lock;

  Spinlock() { pthread_spin_init(&m_mutex, 0); }

  ~Spinlock() { pthread_spin_destroy(&m_mutex); }

  void lock() { pthread_spin_lock(&m_mutex); }

  void unlock() { pthread_spin_unlock(&m_mutex); }

 private:
  /// 自旋锁
  pthread_spinlock_t m_mutex;
};

class CASLock : Noncopyable {
 public:
  /// 局部锁
  typedef ScopedLockImpl<CASLock> Lock;

  CASLock() { m_mutex.clear(); }

  ~CASLock() {}

  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&m_mutex,
                                                  std::memory_order_acquire))
      ;
  }

  void unlock() {
    std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
  }

 private:
  /// 原子状态
  volatile std::atomic_flag m_mutex;
};

// 读写自旋锁
class RWSpinlock : Noncopyable {
 public:
  // 局部读锁
  typedef ReadScopedLockImpl<RWSpinlock> ReadLock;

  // 局部写锁
  typedef WriteScopedLockImpl<RWSpinlock> WriteLock;

  RWSpinlock() {}

  ~RWSpinlock() {}

  void rdlock() { m_lock.lock_shared(); }

  void wrlock() { m_lock.lock(); }

  void unlock() { m_lock.unlock(); }

 private:
  /// 读写锁
  tbb::spin_rw_mutex m_lock;
};

class Scheduler;
class FiberSemaphore : Noncopyable {
 public:
  typedef Spinlock MutexType;

  FiberSemaphore(size_t initial_concurrency = 0);
  ~FiberSemaphore();

  bool tryWait();
  void wait();
  void notify();
  void notifyAll();

  size_t getConcurrency() const { return m_concurrency; }
  void reset() { m_concurrency = 0; }

 private:
  MutexType m_mutex;
  std::list<std::pair<Scheduler*, Fiber::ptr>> m_waiters;
  size_t m_concurrency;
};

}  // namespace sylar

#endif
