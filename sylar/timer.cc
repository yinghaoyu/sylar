#include "timer.h"
#include "util.h"

namespace sylar {

bool Timer::Comparator::operator()(const Timer::ptr& lhs,
                                   const Timer::ptr& rhs) const {
  if (!lhs && !rhs) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  if (!rhs) {
    return false;
  }
  if (lhs->m_next < rhs->m_next) {
    return true;
  }
  if (rhs->m_next < lhs->m_next) {
    return false;
  }
  return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring,
             TimerManager* manager)
    : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager) {
  m_next = sylar::GetCurrentMS() + m_ms;
}

Timer::Timer(uint64_t next) : m_next(next) {}

bool Timer::cancel() {
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (m_cb) {
    m_cb = nullptr;
    auto it = m_manager->m_timers.find(shared_from_this());
    m_manager->m_timers.erase(it);
    return true;
  }
  return false;
}

bool Timer::refresh() {
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_cb) {
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  // 注意这里需要先删除，再改时间
  // 因为定时器是放在 set 里的，我们自定义了排序规则
  // 如果先改时间，这个定时器的顺序就会乱掉，find 就找不到了
  m_manager->m_timers.erase(it);
  m_next = sylar::GetCurrentMS() + m_ms;
  m_manager->m_timers.insert(shared_from_this());
  return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
  if (ms == m_ms && !from_now) {
    return true;
  }
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_cb) {
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  m_manager->m_timers.erase(it);
  uint64_t start = 0;
  if (from_now) {
    start = sylar::GetCurrentMS();
  } else {
    start = m_next - m_ms;
  }
  m_ms = ms;
  m_next = start + m_ms;
  m_manager->addTimer(shared_from_this(), lock);
  return true;
}

TimerManager::TimerManager() {
  m_previouseTime = sylar::GetCurrentMS();
}

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                                  bool recurring) {
  Timer::ptr timer(new Timer(ms, cb, recurring, this));
  RWMutexType::WriteLock lock(m_mutex);
  addTimer(timer, lock);
  return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
  std::shared_ptr<void> tmp = weak_cond.lock();
  if (tmp) {
    cb();
  }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t ms,
                                           std::function<void()> cb,
                                           std::weak_ptr<void> weak_cond,
                                           bool recurring) {
  return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  m_tickled = false;
  if (m_timers.empty()) {
    // 没有定时器任务，返回最大时间戳
    return ~0ull;
  }

  const Timer::ptr& next = *m_timers.begin();
  uint64_t now_ms = sylar::GetCurrentMS();
  if (now_ms >= next->m_next) {
    // 说明有定时器需要超时处理
    return 0;
  } else {
    // 到下一个定时器超时需要等待的时间
    return next->m_next - now_ms;
  }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
  uint64_t now_ms = sylar::GetCurrentMS();
  std::vector<Timer::ptr> expired;  // 已经超时的定时器
  {
    RWMutexType::ReadLock lock(m_mutex);
    if (m_timers.empty()) {
      return;
    }
  }
  RWMutexType::WriteLock lock(m_mutex);

  if (m_timers.empty()) {
    return;
  }

  bool rollover = detectClockRollover(now_ms);
  if (!rollover && ((*m_timers.begin())->m_next > now_ms)) {
    return;
  }

  Timer::ptr now_timer(new Timer(now_ms));
  // 暴力方法，系统时间改过，定时器全部设置成超时
  auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
  while (it != m_timers.end() && (*it)->m_next == now_ms) {
    // lower_bound 是找到第一个 >= now_timer 的位置
    // 定时器可能存在多个时间戳一样的，因此和 now_ms 相等的定时器也要全部处理
    ++it;
  }
  // 把过期的定时器全都加入 expired
  expired.insert(expired.begin(), m_timers.begin(), it);
  m_timers.erase(m_timers.begin(), it);
  cbs.reserve(expired.size());

  for (auto& timer : expired) {
    cbs.push_back(timer->m_cb);
    if (timer->m_recurring) {
      // 循环定时器需再次加入
      timer->m_next = now_ms + timer->m_ms;
      m_timers.insert(timer);
    } else {
      // cb 置 nullptr 表示已经处理过了
      timer->m_cb = nullptr;
    }
  }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
  auto it = m_timers.insert(val).first;
  bool at_front = (it == m_timers.begin()) && !m_tickled;
  if (at_front) {
    // 减少频繁调用 addTimer 导致多次调用 onTimerInsertedAtFront
    m_tickled = true;
  }
  lock.unlock();

  if (at_front) {
    onTimerInsertedAtFront();
  }
}

// 检测服务器是否调整了系统时间
bool TimerManager::detectClockRollover(uint64_t now_ms) {
  bool rollover = false;
  if (now_ms < m_previouseTime && now_ms < (m_previouseTime - 60 * 60 * 1000)) {
    rollover = true;
  }
  m_previouseTime = now_ms;
  return rollover;
}

bool TimerManager::hasTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  return !m_timers.empty();
}

}  // namespace sylar
