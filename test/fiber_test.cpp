#include "../src/fiber.h"
#include "../src/log.h"
#include <iostream>
#include "../src/util.h"
#include "../src/macro.h"

sylar::Logger::ptr logger = SYLAR_LOG_ROOT();

void run_in_fiber(){
    SYLAR_LOG_INFO(logger) << "run_in_fiber begin";
    sylar::Fiber::YieldToHold();
    SYLAR_LOG_INFO(logger) << "run_in_fiber end";
    sylar::Fiber::YieldToHold();
}

int main() {
    sylar::Thread::SetName("main");
    SYLAR_LOG_INFO(logger) << "main begin -1";
    {
        sylar::Fiber::GetThis();
        SYLAR_LOG_INFO(logger) << "main begin";
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
        fiber->swapIn();
        SYLAR_LOG_INFO(logger) << "main after swapIn";
        fiber->swapIn();
        SYLAR_LOG_INFO(logger) << "main after end";
        fiber->swapIn();
    }
    SYLAR_LOG_INFO(logger) << "main after end2";
    return 0;
}