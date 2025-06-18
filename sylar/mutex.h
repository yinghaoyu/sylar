#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include "fiber.h"
#include "noncopyable.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <tbb/spin_rw_mutex.h>
#include <atomic>
#include <list>

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
      m_mutex.lock_shared();
    }
  }

  void unlock() {
    if (m_locked) {
      m_locked = false;
      m_mutex.unlock_shared();
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

class RWMutex : Noncopyable {
 public:
  typedef ReadScopedLockImpl<RWMutex> ReadLock;

  typedef WriteScopedLockImpl<RWMutex> WriteLock;

  RWMutex() { pthread_rwlock_init(&m_lock, nullptr); }

  ~RWMutex() { pthread_rwlock_destroy(&m_lock); }

  void lock_shared() { pthread_rwlock_rdlock(&m_lock); }

  void unlock_shared() { pthread_rwlock_unlock(&m_lock); }

  void lock() { pthread_rwlock_wrlock(&m_lock); }

  void unlock() { pthread_rwlock_unlock(&m_lock); }

 private:
  pthread_rwlock_t m_lock;
};

class Spinlock : Noncopyable {
 public:
  /// 局部锁
  typedef ScopedLockImpl<Spinlock> Lock;

  Spinlock() {}

  ~Spinlock() {}

  void lock() {
    for (;;) {
      if (!m_locked.exchange(true, std::memory_order_acquire)) {
        return;
      }
      while (m_locked.load(std::memory_order_relaxed)) {
        // 让出CPU，避免忙等
        // 这里使用 __builtin_ia32_pause() 是为了在 x86 架构上减少 CPU
        // 的功耗 并且提高多线程程序的性能
        __builtin_ia32_pause();
      }
    }
  }

  bool try_lock() {
    return !m_locked.load(std::memory_order_relaxed) &&
           !m_locked.exchange(true, std::memory_order_acquire);
  }

  void unlock() { m_locked.store(false, std::memory_order_release); }

 private:
  std::atomic_bool m_locked{false};
};

// 读写自旋锁
class RWSpinlock : Noncopyable {
 public:
  typedef ReadScopedLockImpl<RWSpinlock> ReadLock;

  typedef WriteScopedLockImpl<RWSpinlock> WriteLock;

  RWSpinlock() {}

  ~RWSpinlock() {}

  void lock_shared() { m_lock.lock_shared(); }

  void unlock_shared() { m_lock.unlock_shared(); }

  void lock() { m_lock.lock(); }

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
