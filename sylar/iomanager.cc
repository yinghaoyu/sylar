#include "iomanager.h"
#include "log.h"
#include "macro.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

enum EpollCtlOp {};

static std::ostream& operator<<(std::ostream& os, const EpollCtlOp& op) {
  switch ((int)op) {
#define XX(ctl) \
  case ctl:     \
    return os << #ctl;
    XX(EPOLL_CTL_ADD);
    XX(EPOLL_CTL_MOD);
    XX(EPOLL_CTL_DEL);
    default:
      return os << (int)op;
  }
#undef XX
}

static std::ostream& operator<<(std::ostream& os, EPOLL_EVENTS events) {
  if (!events) {
    return os << "0";
  }
  bool first = true;
#define XX(E)       \
  if (events & E) { \
    if (!first) {   \
      os << "|";    \
    }               \
    os << #E;       \
    first = false;  \
  }
  XX(EPOLLIN);
  XX(EPOLLPRI);
  XX(EPOLLOUT);
  XX(EPOLLRDNORM);
  XX(EPOLLRDBAND);
  XX(EPOLLWRNORM);
  XX(EPOLLWRBAND);
  XX(EPOLLMSG);
  XX(EPOLLERR);
  XX(EPOLLHUP);
  XX(EPOLLRDHUP);
  XX(EPOLLONESHOT);
  XX(EPOLLET);
#undef XX
  return os;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(
    IOManager::Event event) {
  switch (event) {
    case IOManager::READ:
      return read;
    case IOManager::WRITE:
      return write;
    default:
      SYLAR_ASSERT2(false, "getContext");
  }
  throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::resetContext(EventContext& ctx) {
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
  ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
  // SYLAR_LOG_INFO(g_logger) << "fd=" << fd
  //     << " triggerEvent event=" << event
  //     << " events=" << events;
  if (SYLAR_UNLIKELY(!(events & event))) {
    SYLAR_LOG_ERROR(g_logger) << "fd=" << fd << " triggerEvent event=" << event
                              << " events=" << events << "\nbacktrace:\n"
                              << sylar::BacktraceToString(100, 2, "    ");
    return;
  }
  events = (Event)(events & ~event);
  EventContext& ctx = getContext(event);
  if (ctx.cb) {
    // schedule 里面用了 swap
    // 调度结束 ctx.cb 就会变成 nullptr
    ctx.scheduler->schedule(&ctx.cb);
  } else {
    ctx.scheduler->schedule(&ctx.fiber);
  }
  ctx.scheduler = nullptr;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    : Scheduler(threads, use_caller, name) {
  m_epfd = epoll_create(5000);
  SYLAR_ASSERT(m_epfd > 0);

  int rt = pipe(m_tickleFds);
  SYLAR_ASSERT(!rt);

  epoll_event event;
  memset(&event, 0, sizeof(epoll_event));
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = m_tickleFds[0];

  rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
  SYLAR_ASSERT(!rt);

  rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
  SYLAR_ASSERT(!rt);

  contextResize(32);

  start();
}

IOManager::~IOManager() {
  stop();
  close(m_epfd);
  close(m_tickleFds[0]);
  close(m_tickleFds[1]);

  for (size_t i = 0; i < m_fdContexts.size(); ++i) {
    if (m_fdContexts[i]) {
      delete m_fdContexts[i];
    }
  }
}

void IOManager::contextResize(size_t size) {
  m_fdContexts.resize(size);

  for (size_t i = 0; i < m_fdContexts.size(); ++i) {
    if (!m_fdContexts[i]) {
      m_fdContexts[i] = new FdContext;
      m_fdContexts[i]->fd = i;
    }
  }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
  FdContext* fd_ctx = nullptr;
  RWMutexType::ReadLock lock(m_mutex);
  if ((int)m_fdContexts.size() > fd) {
    fd_ctx = m_fdContexts[fd];
    lock.unlock();
  } else {
    lock.unlock();
    RWMutexType::WriteLock lock2(m_mutex);
    contextResize(fd * 1.5);
    fd_ctx = m_fdContexts[fd];
  }

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  if (SYLAR_UNLIKELY(fd_ctx->events & event)) {
    // 同一个 fd 添加已存在的事件，没有意义
    SYLAR_LOG_ERROR(g_logger)
        << "addEvent assert fd=" << fd << " event=" << (EPOLL_EVENTS)event
        << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx->events;
    SYLAR_ASSERT(!(fd_ctx->events & event));
  }

  int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  epoll_event epevent;
  epevent.events = EPOLLET | fd_ctx->events | event;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
    // 返回值非 0 表示出错
    SYLAR_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)epevent.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno)
        << ") fd_ctx->events=" << (EPOLL_EVENTS)fd_ctx->events;
    return -1;
  }

  ++m_pendingEventCount;
  fd_ctx->events = (Event)(fd_ctx->events | event);
  FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
  // 首次添加，所有的值应该都是空的
  SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

  event_ctx.scheduler = Scheduler::GetThis();
  if (cb) {
    event_ctx.cb.swap(cb);
  } else {
    event_ctx.fiber = Fiber::GetThis();
    SYLAR_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC,
                  "state=" << event_ctx.fiber->getState());
  }
  return 0;
}

bool IOManager::delEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(m_mutex);
  if ((int)m_fdContexts.size() <= fd) {
    return false;
  }
  FdContext* fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  if (SYLAR_UNLIKELY(!(fd_ctx->events & event))) {
    return false;
  }

  Event new_events = (Event)(fd_ctx->events & ~event);
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
    SYLAR_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)epevent.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  --m_pendingEventCount;
  fd_ctx->events = new_events;
  FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
  fd_ctx->resetContext(event_ctx);
  return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(m_mutex);
  if ((int)m_fdContexts.size() <= fd) {
    return false;
  }
  FdContext* fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  if (SYLAR_UNLIKELY(!(fd_ctx->events & event))) {
    return false;
  }

  Event new_events = (Event)(fd_ctx->events & ~event);
  int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
    SYLAR_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)epevent.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  fd_ctx->triggerEvent(event);
  --m_pendingEventCount;
  return true;
}

bool IOManager::cancelAll(int fd) {
  RWMutexType::ReadLock lock(m_mutex);
  if ((int)m_fdContexts.size() <= fd) {
    return false;
  }
  FdContext* fd_ctx = m_fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fd_ctx->mutex);
  if (!fd_ctx->events) {
    return false;
  }

  int op = EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events = 0;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(m_epfd, op, fd, &epevent);
  if (rt) {
    SYLAR_LOG_ERROR(g_logger)
        << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", " << fd
        << ", " << (EPOLL_EVENTS)epevent.events << "):" << rt << " (" << errno
        << ") (" << strerror(errno) << ")";
    return false;
  }

  if (fd_ctx->events & READ) {
    fd_ctx->triggerEvent(READ);
    --m_pendingEventCount;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->triggerEvent(WRITE);
    --m_pendingEventCount;
  }

  SYLAR_ASSERT(fd_ctx->events == 0);
  return true;
}

IOManager* IOManager::GetThis() {
  return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
  if (!hasIdleThreads()) {
    // 没有空闲的线程，调度也没有意义
    return;
  }
  int rt = write(m_tickleFds[1], "T", 1);
  SYLAR_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout) {
  timeout = getNextTimer();
  return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
  uint64_t timeout = 0;
  return stopping(timeout);
}

void IOManager::idle() {
  SYLAR_LOG_DEBUG(g_logger) << "idle";
  // 因为用的是协程，栈大小设置的可能比较小
  // 最好不要在栈上分配很大的数组
  const uint64_t MAX_EVNETS = 256;
  epoll_event* events = new epoll_event[MAX_EVNETS]();
  std::shared_ptr<epoll_event> shared_events(
      events, [](epoll_event* ptr) { delete[] ptr; });

  while (true) {
    uint64_t next_timeout = 0;  // 最小的定时器的超时时间间隔
    if (SYLAR_UNLIKELY(stopping(next_timeout))) {
      // 已经结束，直接离开
      SYLAR_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
      break;
    }

    int rt = 0;
    do {
      static const int MAX_TIMEOUT = 3000;  // epoll 忙等毫秒级
      if (next_timeout != ~0ull) {
        next_timeout =
            (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
      } else {
        next_timeout = MAX_TIMEOUT;
      }
      rt = epoll_wait(m_epfd, events, MAX_EVNETS, (int)next_timeout);
      if (rt < 0 && errno == EINTR) {
      } else {
        break;
      }
    } while (true);

    std::vector<std::function<void()>> cbs;
    listExpiredCb(cbs);
    if (!cbs.empty()) {
      // 处理超时定时器的回调函数
      // SYLAR_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
      schedule(cbs.begin(), cbs.end());
      cbs.clear();
    }

    // if(SYLAR_UNLIKELY(rt == MAX_EVNETS)) {
    //     SYLAR_LOG_INFO(g_logger) << "epoll wait events=" << rt;
    // }

    // 处理一下事件
    for (int i = 0; i < rt; ++i) {
      epoll_event& event = events[i];
      if (event.data.fd == m_tickleFds[0]) {
        uint8_t dummy[256];
        // tickleFds 设置的是 EPOLLET，因此需要循环读干净
        while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0)
          ;
        continue;
      }

      FdContext* fd_ctx = (FdContext*)event.data.ptr;
      FdContext::MutexType::Lock lock(fd_ctx->mutex);
      if (event.events & (EPOLLERR | EPOLLHUP)) {
        // 错误和中断事件
        event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
      }
      int real_events = NONE;
      if (event.events & EPOLLIN) {
        real_events |= READ;
      }
      if (event.events & EPOLLOUT) {
        real_events |= WRITE;
      }

      if ((fd_ctx->events & real_events) == NONE) {
        // 事件已经被处理
        continue;
      }

      int left_events = (fd_ctx->events & ~real_events);
      int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      event.events = EPOLLET | left_events;

      int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
      if (rt2) {
        SYLAR_LOG_ERROR(g_logger)
            << "epoll_ctl(" << m_epfd << ", " << (EpollCtlOp)op << ", "
            << fd_ctx->fd << ", " << (EPOLL_EVENTS)event.events << "):" << rt2
            << " (" << errno << ") (" << strerror(errno) << ")";
        continue;
      }

      // SYLAR_LOG_INFO(g_logger) << " fd=" << fd_ctx->fd << " events=" <<
      // fd_ctx->events
      //                          << " real_events=" << real_events;
      // 触发事件，关注读写
      if (real_events & READ) {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
      }
      if (real_events & WRITE) {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
      }
    }

    // 让出 idle 协程的执行权

    Fiber::ptr cur = Fiber::GetThis();
    auto raw_ptr = cur.get();
    cur.reset();
    // 执行 swapOut 后
    // 从 scheduler::run() 的 idle_fiber->swapIn() 处返回
    raw_ptr->swapOut();
  }
}

void IOManager::onTimerInsertedAtFront() {
  // 每次有超时时间戳最早的定时器插入，就唤醒一下 epoll_wait
  tickle();
}

}  // namespace sylar
