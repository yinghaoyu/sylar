#include "scheduler.h"
#include "hook.h"
#include "log.h"
#include "macro.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    : m_name(name) {
  SYLAR_ASSERT(threads > 0);

  if (use_caller) {
    sylar::Fiber::GetThis();
    --threads;

    SYLAR_ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    m_rootFiber.reset(NewFiber(std::bind(&Scheduler::run, this), 0, true),
                      FreeFiber);
    sylar::Thread::SetName(m_name);
    // use_caller 线程的主协程作为调度协程
    t_scheduler_fiber = m_rootFiber.get();
    m_rootThread = sylar::GetThreadId();
    m_threadIds.push_back(m_rootThread);
  } else {
    m_rootThread = -1;
  }
  // 不包括 use_caller 线程
  m_threadCount = threads;
}

Scheduler::~Scheduler() {
  SYLAR_ASSERT(m_stopping);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler* Scheduler::GetThis() {
  return t_scheduler;
}

Fiber* Scheduler::GetMainFiber() {
  return t_scheduler_fiber;
}

void Scheduler::start() {
  RWMutexType::WriteLock lock(m_mutex);
  if (!m_stopping) {
    return;
  }
  m_stopping = false;
  SYLAR_ASSERT(m_threads.empty());

  m_threads.resize(m_threadCount);
  for (size_t i = 0; i < m_threadCount; ++i) {
    m_threads[i] = std::make_shared<Thread>(std::bind(&Scheduler::run, this),
                                            m_name + "_" + std::to_string(i));
    m_threadIds.push_back(m_threads[i]->getId());
  }
}

void Scheduler::stop() {
  m_autoStop = true;
  if (m_rootFiber && m_threadCount == 0 &&
      (m_rootFiber->getState() == Fiber::TERM ||
       m_rootFiber->getState() == Fiber::INIT)) {
    SYLAR_LOG_INFO(g_logger) << this << " stopped";
    m_stopping = true;

    if (stopping()) {
      // 可以停止，则立即停止
      return;
    }
  }
  // 还不能立即停止

  if (m_rootThread != -1) {
    // 使用了 use_caller，调度器归属调用线程
    SYLAR_ASSERT(GetThis() == this);
  } else {
    // 否则，调度器归属线程池的线程
    SYLAR_ASSERT(GetThis() != this);
  }

  m_stopping = true;
  for (size_t i = 0; i < m_threadCount; ++i) {
    tickle();
  }

  if (m_rootFiber) {
    tickle();
  }

  if (m_rootFiber) {
    if (!stopping()) {
      // use_caller 的调度协程下还有协程没结束，需要调度执行
      m_rootFiber->call();
      // use_caller 的调度协程下所有协程都结束了，从这里返回
    }
  }

  std::vector<Thread::ptr> thrs;
  {
    RWMutexType::WriteLock lock(m_mutex);
    thrs.swap(m_threads);
  }

  // 等待所有线程结束
  for (auto& i : thrs) {
    i->join();
  }
}

void Scheduler::setThis() {
  t_scheduler = this;
}

void Scheduler::run() {
  SYLAR_LOG_DEBUG(g_logger) << m_name << " run";
  set_hook_enable(true);
  // 调度器归属当前线程
  setThis();
  if (sylar::GetThreadId() != m_rootThread) {
    // 线程池线程的主协程作为调度协程
    t_scheduler_fiber = Fiber::GetThis().get();
  }

  Fiber::ptr idle_fiber(NewFiber(std::bind(&Scheduler::idle, this)), FreeFiber);
  Fiber::ptr cb_fiber;

  FiberAndThread ft;
  while (true) {
    ft.reset();
    bool tickle_me = false;
    bool is_active = false;
    {
      RWMutexType::WriteLock lock(m_mutex);
      auto it = m_fibers.begin();
      while (it != m_fibers.end()) {
        if (it->thread != -1 && it->thread != sylar::GetThreadId()) {
          // 协程指定了线程
          ++it;
          tickle_me = true;  // 需要通知其他线程
          continue;
        }

        SYLAR_ASSERT(it->fiber || it->cb);
        if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
          // 正在执行的协程，不需要调度
          ++it;
          continue;
        }

        ft = *it;
        m_fibers.erase(it++);
        ++m_activeThreadCount;
        is_active = true;
        break;
      }
      tickle_me |= !m_fibers.empty();
    }

    if (tickle_me) {
      tickle();
    }

    if (ft.fiber && (ft.fiber->getState() != Fiber::TERM &&
                     ft.fiber->getState() != Fiber::EXCEPT)) {
      // 协程的形式，调度执行
      ft.fiber->swapIn();
      // 从调度恢复
      --m_activeThreadCount;

      if (ft.fiber->getState() == Fiber::READY) {
        // 恢复后处于 READY，还需要调度执行
        schedule(ft.fiber);
      } else if (ft.fiber->getState() != Fiber::TERM &&
                 ft.fiber->getState() != Fiber::EXCEPT) {
        // 调度结束，把状态改为 HOLD，等待下次调度
        ft.fiber->m_state = Fiber::HOLD;
      }
      ft.reset();
    } else if (ft.cb) {
      // 回调函数的形式
      if (cb_fiber) {
        cb_fiber->reset(ft.cb);
      } else {
        // 给回调函数分配一个协程
        cb_fiber.reset(NewFiber(ft.cb), FreeFiber);
      }
      ft.reset();
      // 调度执行
      cb_fiber->swapIn();
      // 从调度恢复
      --m_activeThreadCount;
      if (cb_fiber->getState() == Fiber::READY) {
        // 恢复后处于 READY，还需要调度执行
        schedule(cb_fiber);
        cb_fiber.reset();
      } else if (cb_fiber->getState() == Fiber::EXCEPT ||
                 cb_fiber->getState() == Fiber::TERM) {
        // 执行结束，不再调度
        cb_fiber->reset(nullptr);
      } else {
        // 调度结束，把状态改为 HOLD，等待下次调度
        cb_fiber->m_state = Fiber::HOLD;
        cb_fiber.reset();
      }
    } else {
      if (is_active) {
        --m_activeThreadCount;
        continue;
      }
      if (idle_fiber->getState() == Fiber::TERM) {
        SYLAR_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++m_idleThreadCount;
      idle_fiber->swapIn();
      --m_idleThreadCount;
      if (idle_fiber->getState() != Fiber::TERM &&
          idle_fiber->getState() != Fiber::EXCEPT) {
        idle_fiber->m_state = Fiber::HOLD;
      }
    }
  }
}

void Scheduler::tickle() {
  SYLAR_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
  RWMutexType::ReadLock lock(m_mutex);
  return m_autoStop && m_stopping && m_fibers.empty() &&
         m_activeThreadCount == 0;
}

void Scheduler::idle() {
  SYLAR_LOG_INFO(g_logger) << "idle";
  while (!stopping()) {
    sylar::Fiber::YieldToHold();
  }
}

void Scheduler::switchTo(int thread) {
  SYLAR_ASSERT(Scheduler::GetThis() != nullptr);
  if (Scheduler::GetThis() == this) {
    if (thread == -1 || thread == sylar::GetThreadId()) {
      // 切换的目标是自己，无需切换
      return;
    }
  }
  schedule(Fiber::GetThis(), thread);
  Fiber::YieldToHold();
}

std::ostream& Scheduler::dump(std::ostream& os) {
  os << "[Scheduler name=" << m_name << " size=" << m_threadCount
     << " active_count=" << m_activeThreadCount
     << " idle_count=" << m_idleThreadCount << " stopping=" << m_stopping
     << " ]" << std::endl
     << "    ";
  for (size_t i = 0; i < m_threadIds.size(); ++i) {
    if (i) {
      os << ", ";
    }
    os << m_threadIds[i];
  }
  return os;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
  m_caller = Scheduler::GetThis();
  if (target) {
    target->switchTo();
  }
}

SchedulerSwitcher::~SchedulerSwitcher() {
  if (m_caller) {
    m_caller->switchTo();
  }
}

}  // namespace sylar
