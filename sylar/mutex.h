#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include "fiber.h"
#include "noncopyable.h"

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <tbb/spin_rw_mutex.h>

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

template <class T> struct ScopedLockImpl {
  public:
    ScopedLockImpl(T &mutex) : m_mutex(mutex) { lock(); }

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
    T &m_mutex;
    /// 是否已上锁
    bool m_locked = false;
};

template <class T> struct ReadScopedLockImpl {
  public:
    ReadScopedLockImpl(T &mutex) : m_mutex(mutex) { lock(); }

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
    T &m_mutex;
    /// 是否已上锁
    bool m_locked = false;
};

template <class T> struct WriteScopedLockImpl {
  public:
    WriteScopedLockImpl(T &mutex) : m_mutex(mutex) { lock(); }

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
    T &m_mutex;
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
    // 基于 Howard Hinnant 的参考实现
    // @sa https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2406.html
  public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;

    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    RWMutex() {}

    ~RWMutex() {
        if (m_state != 0) {
            assert(!"RWMutex destroyed while locked");
        }
    }

    void lock_shared() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond1.wait(lock, [this]() { return m_state < _S_max_readers; });
        ++m_state;
    }

    void unlock_shared() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto pre = m_state--;
        if (write_entered()) {
            if (readers() == 0) {
                m_cond2.notify_one();
            }
        } else {
            if (pre == _S_max_readers) {
                m_cond1.notify_one();
            }
        }
    }

    bool try_lock_shared() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!lock.owns_lock()) {
            return false;
        }
        if (m_state >= _S_max_readers) {
            return false;
        }
        ++m_state;
        return true;
    }

    void lock() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond1.wait(lock, [this]() { return !write_entered(); });
        m_state |= _S_write_entered;
        m_cond2.wait(lock, [this]() { return readers() == 0; });
    }

    void unlock() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = 0;
        m_cond1.notify_all();
    }

    bool try_lock() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!lock.owns_lock()) {
            return false;
        }
        if (m_state != 0) {
            return false;
        }
        m_state |= _S_write_entered;
        return true;
    }

  private:
    static constexpr unsigned _S_write_entered =
        1u << (sizeof(unsigned) * __CHAR_BIT__ - 1);

    static constexpr unsigned _S_max_readers = ~_S_write_entered;

    bool write_entered() const { return m_state & _S_write_entered; }

    bool readers() const { return m_state & _S_max_readers; }

    std::mutex m_mutex;
    // 1. 写者等待写锁释放
    // 2. 读者等待读锁数量未达到上限
    std::condition_variable m_cond1;
    // 1. 写者等待所有读锁释放
    std::condition_variable m_cond2;
    unsigned m_state = 0;
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
    std::list<std::pair<Scheduler *, Fiber::ptr>> m_waiters;
    size_t m_concurrency;
};

} // namespace sylar

#endif
