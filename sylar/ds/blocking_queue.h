#ifndef __SYLAR_DS_BLOCKING_QUEUE_H__
#define __SYLAR_DS_BLOCKING_QUEUE_H__

#include "sylar/mutex.h"

namespace sylar {
namespace ds {

template <class T>
class BlockingQueue {
 public:
  typedef std::shared_ptr<BlockingQueue> ptr;
  typedef std::shared_ptr<T> data_type;

  size_t push(const data_type& data) {
    sylar::Mutex::Lock lock(m_mutex);
    m_datas.push_back(data);
    size_t size = m_datas.size();
    m_sem.notify();  // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
    return size;
  }

  data_type pop() {
    sylar::Mutex::Lock lock(m_mutex);
    // always use a while-loop, due to spurious wakeup
    while (m_datas.empty()) {
      m_sem.wait();
    }
    assert(!m_datas.empty());
    auto v = m_datas.front();
    m_datas.pop_front();
    return v;
  }

  size_t size() {
    sylar::Mutex::Lock lock(m_mutex);
    return m_datas.size();
  }

  bool empty() {
    sylar::Mutex::Lock lock(m_mutex);
    return m_datas.empty();
  }

 private:
  sylar::FiberSemaphore m_sem;
  sylar::Mutex m_mutex;
  std::list<data_type> m_datas;
};

}  // namespace ds
}  // namespace sylar

#endif
