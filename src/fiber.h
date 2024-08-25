#ifndef __SYLAR__FIBER_H__
#define __SYLAR__FIBER_H__

#include <ucontext.h>
#include <memory>
#include <functional>
#include "thread.h"

namespace sylar {

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
friend class Scheduler;
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPTION
    };
private:
    Fiber();
public:
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
    ~Fiber();

    // 重置协程函数，并重置状态，能重置的也就是INIT或者TERM状态才能重置
    void reset(std::function<void()> cb);
    // 切换到当前协程执行
    void swapIn();
    // 切换到后台
    void swapOut();

    void call();
    void back();

    uint64_t GetId() const { return m_id; }
    State getState() const { return m_state;}
public:
    // 设置当前协程
    static void SetThis(Fiber *f);
    // 返回当前协程
    static Fiber::ptr GetThis();
    // 协程切换到后台，并且设置为Ready状态
    static void YieldToReady();
    // 协程切换到后台，并且设置为Hold状态
    static void YieldToHold();
    // 获取总协程数
    static uint64_t TotalFibers();

    static void MainFunc();

    static void CallerMainFunc();

    static uint64_t GetFiberId();

// 这个private都是协程自己的一些函数
private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = INIT;

    ucontext_t m_ctx;
    void *m_stack = nullptr;

    std::function<void()> m_cb;
};

}

#endif