#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__

#include <iostream>
#include <list>
#include <memory>
#include <vector>

#include "fiber.h"
#include "thread.h"

namespace sylar {

// N-M的协程调度器
// 内部有一个线程池,支持协程在线程池里面切换
class Scheduler {
 public:
  typedef std::shared_ptr<Scheduler> ptr;
  typedef RWMutex RWMutexType;

  // threads 线程数量
  // use_caller 是否使用当前调用线程
  // name 协程调度器名称
  Scheduler(size_t threads = 1, bool use_caller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return m_name; }

  static Scheduler* GetThis();
  static Fiber* GetMainFiber();

  void start();
  void stop();

  // 调度协程
  // fc 协程或函数
  // thread 协程执行的线程id,-1标识任意线程
  template <class FiberOrCb>
  void schedule(FiberOrCb fc, int thread = -1) {
    bool need_tickle = false;
    {
      RWMutexType::WriteLock lock(m_mutex);
      need_tickle = scheduleNoLock(fc, thread);
    }

    if (need_tickle) {
      tickle();
    }
  }

  // 批量调度协程
  // begin 协程数组的开始
  // end 协程数组的结束
  template <class InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool need_tickle = false;
    {
      RWMutexType::WriteLock lock(m_mutex);
      while (begin != end) {
        need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
        ++begin;
      }
    }
    if (need_tickle) {
      tickle();
    }
  }

  // 把调用 switchTo 的协程切换到指定线程运行
  // thread 协程执行的线程id,-1标识任意线程
  void switchTo(int thread = -1);

  std::ostream& dump(std::ostream& os);

 protected:
  // 通知协程调度器有任务了
  virtual void tickle();

  // 协程调度函数
  void run();

  // 返回是否可以停止
  virtual bool stopping();

  // 协程无任务可调度时执行idle协程
  virtual void idle();

  // 设置当前的协程调度器
  void setThis();

  // 是否有空闲线程
  bool hasIdleThreads() { return m_idleThreadCount > 0; }

 private:
  // 协程调度启动 thread unsafe
  template <class FiberOrCb>
  bool scheduleNoLock(FiberOrCb fc, int thread) {
    bool need_tickle = m_fibers.empty();
    FiberAndThread ft(fc, thread);
    if (ft.fiber || ft.cb) {
      m_fibers.push_back(ft);
    }
    return need_tickle;
  }

 private:
  // 协程单元
  struct FiberAndThread {
    Fiber::ptr fiber;          // 协程
    std::function<void()> cb;  // 协程执行函数
    int thread;                // 协程归属的线程id

    FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}

    FiberAndThread(Fiber::ptr* f, int thr) : thread(thr) { fiber.swap(*f); }

    FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}

    FiberAndThread(std::function<void()>* f, int thr) : thread(thr) {
      cb.swap(*f);
    }

    FiberAndThread() : thread(-1) {}

    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

 private:
  RWMutexType m_mutex;
  std::vector<Thread::ptr> m_threads;  // 线程池
  std::list<FiberAndThread> m_fibers;  // 待执行的协程队列
  Fiber::ptr m_rootFiber;              // 调度协程(use_caller)
  std::string m_name;                  // 协程调度器名称

 protected:
  std::vector<int> m_threadIds;                   // 协程
  size_t m_threadCount = 0;                       // 线程池的线程数
  std::atomic<size_t> m_activeThreadCount = {0};  // 工作线程数
  std::atomic<size_t> m_idleThreadCount = {0};    // 空闲线程数
  bool m_stopping = true;                         // 是否正在停止
  bool m_autoStop = false;                        // 是否手动停止
  int m_rootThread = 0;                           // 主线程id(use_caller)
};

class SchedulerSwitcher : public Noncopyable {
 public:
  SchedulerSwitcher(Scheduler* target = nullptr);
  ~SchedulerSwitcher();

 private:
  Scheduler* m_caller;
};

}  // namespace sylar

#endif
