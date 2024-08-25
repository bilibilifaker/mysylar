#include "../src/scheduler.h"
#include "../src/log.h"

sylar::Logger::ptr scheduler_logger = SYLAR_LOG_ROOT();

void test_fiber() {
    SYLAR_LOG_INFO(scheduler_logger) << "in test fiber";

    sleep(1);
    static int s_count = 5;
    if(--s_count >= 0) {
        sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
    }
}

int main() {
    SYLAR_LOG_INFO(scheduler_logger) << "main";
    sylar::Scheduler sc(3, false, "test");
    sc.start();
    sleep(2);
    SYLAR_LOG_INFO(scheduler_logger) << "schedule";
    sc.schedule(&test_fiber);
    sc.stop();
    SYLAR_LOG_INFO(scheduler_logger) << "over";
    return 0;
}