#ifndef __SYLAR_WORKER_H__
#define __SYLAR_WORKER_H__

#include "iomanager.h"
#include "log.h"
#include "mutex.h"
#include "singleton.h"

namespace sylar {

class WorkerGroup : Noncopyable,
                    public std::enable_shared_from_this<WorkerGroup> {
 public:
  typedef std::shared_ptr<WorkerGroup> ptr;
  static WorkerGroup::ptr Create(
      uint32_t batch_size, sylar::Scheduler* s = sylar::Scheduler::GetThis()) {
    return std::make_shared<WorkerGroup>(batch_size, s);
  }

  WorkerGroup(uint32_t batch_size,
              sylar::Scheduler* s = sylar::Scheduler::GetThis());
  ~WorkerGroup();

  void schedule(std::function<void()> cb, int thread = -1);
  void waitAll();

 private:
  void doWork(std::function<void()> cb);

 private:
  uint32_t m_batchSize;
  bool m_finish;
  Scheduler* m_scheduler;
  FiberSemaphore m_sem;
};

class TimedWorkerGroup : Noncopyable,
                         public std::enable_shared_from_this<TimedWorkerGroup> {
 public:
  typedef std::shared_ptr<TimedWorkerGroup> ptr;
  static TimedWorkerGroup::ptr Create(
      uint32_t batch_size, uint32_t wait_ms,
      sylar::IOManager* s = sylar::IOManager::GetThis());

  TimedWorkerGroup(uint32_t batch_size, uint32_t wait_ms,
                   sylar::IOManager* s = sylar::IOManager::GetThis());
  ~TimedWorkerGroup();

  void schedule(std::function<void()> cb, int thread = -1);
  void waitAll();

 private:
  void doWork(std::function<void()> cb);
  void start();
  void onTimer();

 private:
  uint32_t m_batchSize;
  bool m_finish;
  bool m_timedout;
  uint32_t m_waitTime;
  sylar::Timer::ptr m_timer;
  IOManager* m_iomanager;
  FiberSemaphore m_sem;
};

class WorkerManager {
 public:
  WorkerManager();
  void add(Scheduler::ptr s);
  Scheduler::ptr get(const std::string& name);
  IOManager::ptr getAsIOManager(const std::string& name);

  template <class FiberOrCb>
  void schedule(const std::string& name, FiberOrCb fc, int thread = -1) {
    auto s = get(name);
    if (s) {
      s->schedule(fc, thread);
    } else {
      static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
      SYLAR_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
    }
  }

  template <class Iter>
  void schedule(const std::string& name, Iter begin, Iter end) {
    auto s = get(name);
    if (s) {
      s->schedule(begin, end);
    } else {
      static sylar::Logger::ptr s_logger = SYLAR_LOG_NAME("system");
      SYLAR_LOG_ERROR(s_logger) << "schedule name=" << name << " not exists";
    }
  }

  bool init();
  bool init(const std::map<std::string, std::map<std::string, std::string>>& v);
  void stop();

  bool isStoped() const { return m_stop; }
  std::ostream& dump(std::ostream& os);

  uint32_t getCount();

 private:
  void add(const std::string& name, Scheduler::ptr s);

 private:
  std::map<std::string, std::vector<Scheduler::ptr>> m_datas;
  bool m_stop;
};

typedef sylar::Singleton<WorkerManager> WorkerMgr;

}  // namespace sylar

#endif
