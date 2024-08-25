#include "IOManager.h"
#include "macro.h"
#include <unistd.h>
#include <sys/epoll.h>
#include "log.h"
#include <fcntl.h>
#include <string.h>
#include "scheduler.h"

namespace sylar {

static sylar::Logger::ptr IOManager_logger = SYLAR_LOG_NAME("system");

IOManager::FdContext::EventContext &IOManager::FdContext::getContext(Event event) {
    switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "getContext");
    }
}

void IOManager::FdContext::resetContext(EventContext &ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
    SYLAR_ASSERT(m_events & event);
    m_events = (Event)(m_events & ~event);
    EventContext &ctx = getContext(event);
    if(ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
    return;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    :Scheduler(threads, use_caller, name) {
    m_epoll_fd = epoll_create(5000);
    SYLAR_ASSERT(m_epoll_fd > 0);

    int rt = pipe(m_tickleFds);
    SYLAR_ASSERT(rt == 0);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_tickleFds[0];

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
    SYLAR_ASSERT(rt == 0);

    rt = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    SYLAR_ASSERT(rt == 0);

    contextResize(32);

    start();
}

IOManager::~IOManager(){
    stop();
    close(m_epoll_fd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0; i < m_fdContexts.size(); i++) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i<m_fdContexts.size(); i++) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

// 0 success, -1 error
int IOManager::addEvent(int fd, Event event, std::function<void()> cb){
    FdContext *fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd) {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else {
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);
        fd_ctx = m_fdContexts[fd];
    }

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    // #   define SYLAR_UNLIKELY(x)     __builtin_expect(!!(x), 0)
    // if(SYLAR_UNLIKELY(fd_ctx->m_events & event)) {
    if((fd_ctx->m_events & event)) {
        SYLAR_LOG_ERROR(IOManager_logger) << "addEvent assert fd = " << fd << ", event = " << event << ", fd_ctx.event" << fd_ctx->m_events;
        SYLAR_ASSERT(!(fd_ctx->m_events & event));
    }

    int op = fd_ctx->m_events != 0 ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->m_events | event;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(IOManager_logger) << "epoll_ctl (" << m_epoll_fd << ", " << op << "," << fd << "," << epevent.events << "):" << rt << "(" << errno << ") (" << strerror(errno) << ")"; 
        return -1;
    }

    m_pendingEventCount++;
    fd_ctx->m_events = (Event)(fd_ctx->m_events | event);
    FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
    SYLAR_ASSERT(!event_ctx.scheduler);
    SYLAR_ASSERT(!event_ctx.fiber);
    SYLAR_ASSERT(!event_ctx.cb);

    event_ctx.scheduler = Scheduler::GetThis();
    if (cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();
        SYLAR_ASSERT(event_ctx.fiber->getState() == Fiber::EXEC);
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->m_events & event)) {
        return false;
    }

    Event new_events = (Event) (fd_ctx->m_events & ~event);
    int op = new_events != 0 ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = &fd_ctx;

    int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(IOManager_logger) << "epoll_ctl (" << m_epoll_fd << ", " << op << "," << fd << "," << epevent.events << "):" << rt << "(" << errno << ") (" << strerror(errno) << ")"; 
        return false;
    }

    m_pendingEventCount--;
    fd_ctx->m_events = new_events;
    FdContext::EventContext &event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lcok(fd_ctx->mutex);
    if(!(fd_ctx->m_events & event)) {
        return false;
    }

    Event new_events = (Event) (fd_ctx->m_events & ~event);
    int op = new_events != 0 ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = &fd_ctx;

    int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(IOManager_logger) << "epoll_ctl (" << m_epoll_fd << ", " << op << "," << fd << "," << epevent.events << "):" << rt << "(" << errno << ") (" << strerror(errno) << ")"; 
        return false;
    }
    fd_ctx->triggerEvent(event);
    m_pendingEventCount--;
    return true;
}

bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock lock(m_mutex);
    if ((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext *fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->m_events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = &fd_ctx;

    int rt = epoll_ctl(m_epoll_fd, op, fd, &epevent);
    if(rt) {
        SYLAR_LOG_ERROR(IOManager_logger) << "epoll_ctl (" << m_epoll_fd << ", " << op << "," << fd << "," << epevent.events << "):" << rt << "(" << errno << ") (" << strerror(errno) << ")"; 
        return false;
    }

    if(fd_ctx->m_events & READ) {
        fd_ctx->triggerEvent(READ);
        m_pendingEventCount--;
    }

    if(fd_ctx->m_events & WRITE) {
        fd_ctx->triggerEvent(Event::WRITE);
        m_pendingEventCount--;
    }

    SYLAR_ASSERT(fd_ctx->m_events == 0);
    return true;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    if(hasIdleThreads()) {
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    SYLAR_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t &timeout) {
    timeout = getNextTimer();
    return timeout == UINT64_MAX && m_pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}

void IOManager::idle() {
    epoll_event *events = new epoll_event[64]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr){
        delete[] ptr;
    });

    while(true) {
        uint64_t next_timeout = 0;
        if(stopping(next_timeout)) {
            SYLAR_LOG_INFO(IOManager_logger) << "name = " << getName() << ", idle stopping exit";
            break;
        }

        int rt = 0;
        do {
            next_timeout = getNextTimer();
            static const int MAX_TIMEOUT = 5000;
            if(next_timeout != UINT64_MAX) {
                next_timeout = std::min(MAX_TIMEOUT, (int)next_timeout);
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epoll_fd, events, 64, (int)next_timeout);

            if (rt < 0 && errno == EINTR) {

            } else {
                break;
            }
        } while(true);
    
        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()) {
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        for(int i = 0; i < rt; i++) {
            epoll_event &event = events[i];
            if(event.data.fd == m_tickleFds[0]) {
                uint8_t dummy;
                while(read(m_tickleFds[0], &dummy, 1) == 1) {}
                continue;
            }

            FdContext *fd_ctx = (FdContext *)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);

            if(event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= EPOLLIN | EPOLLOUT;
            }
            int real_events = NONE;
            if (event.events & EPOLLIN) {
                real_events |= READ;
            }
            if(event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            if ((fd_ctx->m_events & real_events) == NONE) {
                continue;
            }
        
            int left_events = (fd_ctx->m_events & ~real_events);
            int op = left_events != 0 ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epoll_fd, op, fd_ctx->fd, &event);
            if (rt2) {
                SYLAR_LOG_ERROR(IOManager_logger) << "epoll_ctl (" << m_epoll_fd << ", " << op << "," << fd_ctx->fd << "," << event.events << "):" << rt2 << "(" << errno << ") (" << strerror(errno) << ")"; 
                continue;
            }

            if(real_events & READ) {
                fd_ctx->triggerEvent(READ);
                m_pendingEventCount--;
            }
            if(real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                m_pendingEventCount--;
            }
        }

        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront(){
    tickle();
}

}