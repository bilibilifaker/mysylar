#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include <thread>
#include <set>
#include <functional>
#include "thread.h"
#include <vector>

namespace sylar {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public :
    typedef std::shared_ptr<Timer> ptr;

private:
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);
    Timer(uint64_t next);

public:
    bool cancel();
    bool refresh();
    bool reset(uint64_t ms, bool from_now);
private:
    bool m_recurring = false;    // 是否是循环定时器
    uint64_t m_ms= 0;                // 执行时间间隔
    uint64_t m_next = 0;         // 下次执行的具体时间
    std::function<void()> m_cb;
    TimerManager *m_manager = nullptr;
private:
    struct Comparator{
        bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
};

class TimerManager{
friend class Timer;
public:
    typedef std::shared_ptr<TimerManager> ptr;
    typedef RWMutex RWMutexType;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

    uint64_t getNextTimer();

    bool hasTimer();

    void listExpiredCb(std::vector<std::function<void()>> &cbs);
protected:
    virtual void onTimerInsertedAtFront() = 0;

    void addTimer(Timer::ptr val, RWMutexType::WriteLock &lock);
private:
    bool detectClockRollover(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    bool m_tickled = false;
    uint64_t m_previouseTime = 0;
};


}

#endif