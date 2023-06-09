#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <functional>
#include <memory>

#define FIBER_UCONTEXT 1
#define FIBER_FCONTEXT 2
#define FIBER_LIBCO 3
#define FIBER_LIBACO 4

#ifndef FIBER_CONTEXT_TYPE
#define FIBER_CONTEXT_TYPE FIBER_LIBACO
#endif

#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
#include <ucontext.h>
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
#include "sylar/fcontext/fcontext.h"
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
#include "sylar/libco/coctx.h"
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
#include "sylar/libaco/aco.h"
#endif

namespace sylar {
class Scheduler;
class Fiber;

Fiber* NewFiber();
Fiber* NewFiber(std::function<void()> cb, size_t stacksize = 0,
                bool use_caller = false);
void FreeFiber(Fiber* ptr);

class Fiber : public std::enable_shared_from_this<Fiber> {
  friend class Scheduler;
  friend Fiber* NewFiber();
  friend Fiber* NewFiber(std::function<void()> cb, size_t stacksize,
                         bool use_caller);
  friend void FreeFiber(Fiber* ptr);

 public:
  typedef std::shared_ptr<Fiber> ptr;

  enum State {
    // 初始状态
    INIT,
    // 暂停状态
    HOLD,
    // 执行中状态
    EXEC,
    // 结束状态
    TERM,
    // 可执行状态
    READY,
    // 异常状态
    EXCEPT
  };

 private:
  Fiber();

 public:
  // cb 协程执行的函数
  // stacksize 协程栈大小
  // use_caller 是否在 MainFiber 上调度
  Fiber(std::function<void()> cb, size_t stacksize = 0,
        bool use_caller = false);
  ~Fiber();

  // 重置协程函数，并重置状态
  // INIT，TERM
  void reset(std::function<void()> cb);
  // 切换到当前协程执行
  void swapIn();
  // 切换到后台执行
  void swapOut();

  // 将当前线程切换到执行状态
  // 执行的为当前线程的主协程
  void call();

  // 将当前线程切换到后台
  // 执行的为该协程
  // 返回到线程的主协程
  void back();

  // 返回协程id
  uint64_t getId() const { return m_id; }

  // 返回协程状态
  State getState() const { return m_state; }

 public:
  // 设置当前线程运行的协程
  static void SetThis(Fiber* f);
  // 返回当前所在的协程
  static Fiber::ptr GetThis();
  // 将当前协程切换到后台，并且设置为Ready状态
  static void YieldToReady();
  // 将当前协程切换到后台，并且设置为Hold状态
  static void YieldToHold();
  // 总协程数
  static uint64_t TotalFibers();

  // 协程执行函数
  // 执行完成返回到线程主协程
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
  static void MainFunc();
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
  static void MainFunc(intptr_t vp);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
  static void* MainFunc(void*, void*);
#endif

  // 协程执行函数
  // 执行完成返回到线程调度协程
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT || FIBER_CONTEXT_TYPE == FIBER_LIBACO
  static void CallerMainFunc();
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
  static void CallerMainFunc(intptr_t vp);
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
  static void* CallerMainFunc(void*, void*);
#endif

  // 获取当前协程的id
  static uint64_t GetFiberId();

 private:
  /// 协程id
  uint64_t m_id = 0;
  /// 协程运行栈大小
  uint32_t m_stacksize = 0;
  /// 协程状态
  State m_state = INIT;
  /// 协程运行函数
  std::function<void()> m_cb;
  /// 协程上下文
#if FIBER_CONTEXT_TYPE == FIBER_UCONTEXT
  ucontext_t m_ctx;
#elif FIBER_CONTEXT_TYPE == FIBER_FCONTEXT
  fcontext_t m_ctx = nullptr;
#elif FIBER_CONTEXT_TYPE == FIBER_LIBCO
  coctx_t m_ctx;
#elif FIBER_CONTEXT_TYPE == FIBER_LIBACO
  aco_t* m_ctx = nullptr;
  aco_share_stack_t m_astack;
#endif
  char m_stack[0];
};

}  // namespace sylar

#endif
