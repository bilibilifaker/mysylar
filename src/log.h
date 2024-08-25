#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <stdint.h>
#include <list>
#include <memory>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <iostream>
#include <string>
#include "util.h"
#include "thread.h"
#include "singleton.h"

#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name);


namespace sylar{

class LogLevel{
    public:
        enum Level{
            UNKNOW = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERROR = 4,
            FATAL = 5
        };

        static std::string ToString(LogLevel::Level level);
        static LogLevel::Level FromString(const std::string &str);
};

class Logger;
class LoggerManager;

// 这个类感觉就是记录了所有需要记录的信息，除了LogLevel
class LogEvent{
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, 
                const char* file, int32_t m_line, uint32_t elapse,
                uint32_t thread_id, uint32_t fiber_id, uint64_t time,
                const std::string &thread_name);
        // ~LogEvent();

        const char* getFile() const{ return m_file; }
        int32_t getLine() const { return m_line; }
        uint32_t getElapse() const { return m_elapse; }
        uint32_t getThreadId() const { return m_threadId; }
        uint32_t getFiberId() const { return m_fiberId; }
        uint64_t getTime() const { return m_time; }
        std::string getContent() const {return m_ss.str(); }
        const std::string getThreadName() const { return m_threadName; }
        std::shared_ptr<Logger> getLogger() const {return m_logger;}
        LogLevel::Level getLevel() const {return m_level;}

        std::stringstream& getSS() { return m_ss; }
        void format(const char* fmt, ...);
        void format(const char* fmt, va_list al);
    private:
        const char* m_file = nullptr;  // 文件名
        int32_t m_line = 0;            // 行号
        uint32_t m_elapse = 0;         // 程序启动开始到现在的ms数
        uint32_t m_threadId = 0;       // 线程id
        uint32_t m_fiberId = 0;        // 协程id
        uint64_t m_time = 0;           // 时间戳
        std::string m_threadName;      // 线程名字
        std::stringstream m_ss;

        std::shared_ptr<Logger> m_logger;
        LogLevel::Level m_level;
};

// 这个类是个日志格式器，传入一个event，就解析出最终需要log的string
class LogFormatter{
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(const std::string &pattern = "[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%N%T%F%T[%p]%T%f:%l%T%m%n");

        std::string format(LogEvent::ptr event);
        void init();
        bool isError() const { return m_error; }
        const std::string getPattern() const { return m_pattern; }
    public:
        class FormatItem{
            public:
                typedef std::shared_ptr<FormatItem> ptr;
                virtual ~FormatItem(){}
                virtual void format(std::ostream &os, LogEvent::ptr event) = 0;
        };
    // debug用了 之后给这个public改为private
    public:
        std::string m_pattern;
        std::vector<FormatItem::ptr> m_items;
        bool m_error = false;
};

// 这个类的主要功能是
class LogAppender{
    friend class Logger;
    public:
        typedef std::shared_ptr<LogAppender> ptr;
        
        LogAppender() {
            m_formatter = std::make_shared<LogFormatter>();
        }
        
        virtual ~LogAppender(){}

        virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;

        void setFormatter(LogFormatter::ptr val) { 
            m_formatter = val;
            if(m_formatter) {
                m_hasFormatter = true;
            } else {
                m_hasFormatter = false;
            }
        }

        LogFormatter::ptr getFormatter() {
            Mutex::Lock lock(m_mutex);
            return m_formatter;
        }
        LogLevel::Level getLevel() const {return m_level;}
        void setLevel(LogLevel::Level val) {m_level = val;}

        virtual std::string toYamlString() = 0;
    // debug用了，这个public之后要改为protected
    public:
        LogLevel::Level m_level = LogLevel::DEBUG;
        bool m_hasFormatter = false;
        Mutex m_mutex;
        LogFormatter::ptr m_formatter;
};

class Logger : public std::enable_shared_from_this<Logger> {
    friend class LoggerManager;
    public:
        typedef std::shared_ptr<Logger> ptr;
        std::string toYamlString();

        Logger(const std::string &name = "root");
        void log(LogLevel::Level level, LogEvent::ptr event);

        // void debug(LogEvent::ptr event);
        // void info(LogEvent::ptr event);
        // void warn(LogEvent::ptr event);
        // void error(LogEvent::ptr event);
        // void fatal(LogEvent::ptr event);

        void addAppender(LogAppender::ptr appender);
        void delAppender(LogAppender::ptr appender);

        void clearAppenders();

        LogLevel::Level getLevel() const {return m_level;}
        void setLevel(LogLevel::Level val) {m_level = val;}

        const std::string& getName() const{return m_name;}
        void setFormatter(LogFormatter::ptr val);
        void setFormatter(const std::string &val);
        LogFormatter::ptr getFormatter() ;
    // debug用了，之后改为private
    public:
        std::string m_name;
        LogLevel::Level m_level;
        sylar::Mutex m_mutex;
        std::list<LogAppender::ptr> m_appender;
        LogFormatter::ptr m_formatter;
        Logger::ptr m_root;
};


class StdoutLogAppender : public LogAppender{
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void log(LogLevel::Level level, LogEvent::ptr event) override;
        // 临时debug的一个函数
        void debug(){std::cout << m_formatter->m_pattern << std::endl;}
        virtual std::string toYamlString();
};

class FileLogAppender : public LogAppender{
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
        FileLogAppender(const std::string &filename);
        void log(LogLevel::Level level, LogEvent::ptr event) override;

        bool reopen();
        virtual std::string toYamlString();
    private:
        std::string m_filename;
        std::ofstream m_filestream;
};

class LoggerManager{
    public:
        LoggerManager();
        Logger::ptr getLogger(const std::string& name);

        void init();

        Logger::ptr getRoot() const { return m_root; }

        std::string toYamlString();
    private:
        Mutex m_mutex;
        std::map<std::string, Logger::ptr> m_loggers;
        Logger::ptr m_root;
};


typedef sylar::Singleton<LoggerManager> LoggerMgr;

class LogEventWrap{
    public:
        LogEventWrap(LogEvent::ptr e);
        ~LogEventWrap();

        std::stringstream &getSS();
    private:
        LogEvent::ptr m_event;
};

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()

#define SYLAR_LOG_LEVEL(logger , level) \
    if(logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, \
            level, __FILE__, __LINE__, 0, \
            sylar::GetThreadId(), sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getSS()
 
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)


#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLeve() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(Logger, level, \
                        __FILE__, __LINE__, 0, sylar::GetThreadId(), \
                        sylar::GetFiberId(), time(0), sylar::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)

class MessageFormatItem : public LogFormatter::FormatItem{
    public:
        MessageFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getContent();
        }
};

class LevelFormatItem : public LogFormatter::FormatItem{
    public:
        LevelFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << LogLevel::ToString(event->getLevel());
        }
};

class ElapseFormatItem : public LogFormatter::FormatItem{
    public:
        ElapseFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getElapse();
        }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem{
    public:
        ThreadIdFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getThreadId();
        }
};

class FiberIdFormatItem : public LogFormatter::FormatItem{
    public:
        FiberIdFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getFiberId();
        }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem{
    public:
        ThreadNameFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getThreadName();
        }
};

class DateTimeFormatItem : public LogFormatter::FormatItem{
    public:
        DateTimeFormatItem(const std::string &str = "%Y:%m:%d %H:%M:%S")
            :m_format(str){
                if(m_format.empty()){
                    m_format = "%Y:%m:%d %H:%M:%S";
                }
            }
        void format(std::ostream &os, LogEvent::ptr event) override{
            struct tm tm;
            time_t time = event->getTime();
            localtime_r(&time, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), m_format.c_str(), &tm);
            os << buf;
        }
    private:
        std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem{
    public:
        FilenameFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getFile();
        }
};

class LineFormatItem : public LogFormatter::FormatItem{
    public:
        LineFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << event->getLine();
        }
};

class NewLineFormatItem : public LogFormatter::FormatItem{
    public:
        NewLineFormatItem(const std::string &str = ""){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << std::endl;
        }
};

class StringFormatItem : public LogFormatter::FormatItem{
    public:
        StringFormatItem(const std::string &str = "") : m_string(str){}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << m_string;
        }
    private:
        std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem{
    public:
        TabFormatItem(const std::string& str = "")
            :m_string(str) {}
        void format(std::ostream &os, LogEvent::ptr event) override{
            os << "\t";
        }
    private:
        std::string m_string;
};

};


#endif