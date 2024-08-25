#include <iostream>
#include <time.h>
#include "../src/log.cpp"

using namespace std;

int main(){
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter());

    sylar::Logger::ptr logger(new sylar::Logger);
    sylar::StdoutLogAppender::ptr stdout_appender(new sylar::StdoutLogAppender);
    stdout_appender->setFormatter(fmt);
    stdout_appender->setLevel(sylar::LogLevel::INFO);

    logger->addAppender(stdout_appender);

    sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    logger->addAppender(file_appender);
    file_appender->setFormatter(fmt);
    file_appender->setLevel(sylar::LogLevel::INFO);
    
    SYLAR_LOG_INFO(logger) << "test ineri";

    logger->log(sylar::LogLevel::ERROR, sylar::LogEvent::ptr(new sylar::LogEvent(logger, sylar::LogLevel::ERROR, __FILE__, __LINE__, 0, sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::Thread::GetName()))); \
    SYLAR_LOG_INFO(logger) << "test ineri";
    return 0;
    // sylar::Logger::ptr g_logger(new sylar::Logger);
    // logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    // sylar::FileLogAppender::ptr file_appender(new syalr::FileLogAppender("./log.txt"));
    // sylar::LogFormatter::ptr fmt(new syalr::LogFormatter("%d%T%p%T%m%n"));
    // file_appender->setFormatter(fmt);
    // file_appender->setLevel(sylar::LogLevel::ERROR);

    // logger->addAppender(file_appender);

    // std::cout << "hello sylar log" << std::endl;

    // SYLAR_LOG_INFO(logger) << "test macro";
    // SYLAR_LOG_ERROR(logger) << "test macro error";

    // SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    // auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
    // SYLAR_LOG_INFO(l) << "xxx";
    // return 0;
}