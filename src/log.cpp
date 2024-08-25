#include "log.h"
#include <map>
#include <iostream>
#include <functional>
#include <cstdarg> 
#include <string>
#include "config.cpp"
#include "util.cpp"
#include "fiber.cpp"
#include "thread.cpp"
#include "scheduler.cpp"
#include "IOManager.cpp"
#include "timer.cpp"
#include "hook.cpp"
#include "fd_manager.cpp"
#include "address.cpp"

namespace sylar{

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* file,
                    int32_t line, uint32_t elapse,
                    uint32_t thread_id, uint32_t fiber_id, uint64_t time, const std::string &thread_name) 
        :m_file(file)
        ,m_line(line)
        ,m_elapse(elapse)
        ,m_threadId(thread_id)
        ,m_fiberId(fiber_id)
        ,m_time(time) 
        ,m_threadName(thread_name)
        ,m_logger(logger)
        ,m_level(level){
}

void LogEvent::format(const char* fmt, ...){
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
    char *buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1){
        m_ss << std::string(buf, len);
        free(buf);
    }
}



std::string LogLevel::ToString(LogLevel::Level level){
    std::string result;
    switch (level)
    {
        case DEBUG:
            result = "DEBUG";
            break;
        case INFO:
            result = "INFO";
            break;
        case WARN:
            result = "WARN";
            break;
        case ERROR:
            result = "ERROR";
            break;
        case FATAL:
            result = "FATAL";
            break;
        case UNKNOW:
            result = "UNKNOW";
            break;
    }
    return result;
}

/**
 * %p 输出日志等级
 * %f 输出文件名
 * %l 输出行号
 * %d 输出日志时间
 * %t 输出线程号
 * %F 输出协程号
 * %m 输出日志消息
 * %n 输出换行
 * %% 输出百分号
 * %T 输出制表符
 * */
Logger::Logger(const std::string &name)
    :m_name(name)
    ,m_level(LogLevel::DEBUG){
        m_formatter.reset(new LogFormatter("[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%N%T%F%T[%p]%T%f:%l%T%m%n"));

    if(name == "root") {
        m_appender.push_back(LogAppender::ptr(new StdoutLogAppender));
    }
}

void Logger::addAppender(LogAppender::ptr appender){
    Mutex::Lock lock(m_mutex);
    if(!appender->getFormatter()){
        Mutex::Lock ll(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appender.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender){
    Mutex::Lock lock(m_mutex);
    for(auto i = m_appender.begin(); i != m_appender.end(); ++i){
        if(*i == appender){
            m_appender.erase(i);
            break;
        }
    }
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event){
    if(level > m_level){
        auto self = shared_from_this();
        Mutex::Lock lock(m_mutex);
        if( !m_appender.empty())
        {
            for(auto &i : m_appender){
                // std::cout << i->m_formatter->m_pattern  << std::endl;
                i->log(event->getLevel(), event);
            }
        } else if (m_root){
            m_root->log(event->getLevel(), event);
        }
    }
}

// void Logger::debug(LogEvent::ptr event){
//     debug(LogLevel::DEBUG, event);
// }

// void Logger::info(LogEvent::ptr event){
//     debug(LogLevel::INFO, event);
// }

// void Logger::warn(LogEvent::ptr event){
//     debug(LogLevel::WARN, event);
// }

// void Logger::error(LogEvent::ptr event){
//     debug(LogLevel::ERROR, event);
// }

// void Logger::fatal(LogEvent::ptr event){
//     debug(LogLevel::FATAL, event);
// }

std::string LogFormatter::format(LogEvent::ptr event){
    std::stringstream ss;
    for(auto &i : m_items){
        i->format(ss, event);
    }
    return ss.str();
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : m_filename(filename){
    reopen();
}

void FileLogAppender::log(LogLevel::Level level, LogEvent::ptr event) {
    Mutex::Lock lock(m_mutex);
    if(level >= m_level) {
        m_filestream << m_formatter->format(event);
    }
}

bool FileLogAppender::reopen(){
    Mutex::Lock lock(m_mutex);
    if (m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream;
}

void StdoutLogAppender::log(LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level){
        Mutex::Lock lock(m_mutex);
        // std::cout << "333" << std::endl;
        // std::cout << m_formatter->m_pattern << std::endl;
        // std::cout << m_formatter->m_items.size() << std::endl;
        std::cout << m_formatter->format(event);
        std::cout.flush();
    }
}

LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e){}

LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}

std::stringstream& LogEventWrap::getSS(){
    return m_event->getSS();
}


LogFormatter::LogFormatter(const std::string& pattern)
    :m_pattern(pattern) {
    init();
}



void LogFormatter::init(){
    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::string nstr;
    for(size_t i = 0;i<m_pattern.size();i++){
        if(m_pattern[i] != '%'){
            nstr.append(1, m_pattern[i]);  
            continue;
        }
        // 这里就是%%解析成为一个%
        if((i+1) < m_pattern.size()){
            if(m_pattern[i+1] == '%'){
                nstr.append(1,'%');
                continue;
            }
        }

        size_t n = i+1;
        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;
        while(n < m_pattern.size()){
            if(fmt_status == 0 && !isalpha(m_pattern[n]) && m_pattern[n] != '{' && m_pattern[i] != '}'){
                break;
            }
            if(fmt_status == 0){
                if(m_pattern[n] == '{'){
                    str = m_pattern.substr(i+1, n-i-1);
                    fmt_status = 1;
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            }
            if(fmt_status == 1){
                if(m_pattern[n] == '}'){
                    fmt = m_pattern.substr(fmt_begin+1, n-fmt_begin-1);
                    fmt_status = 2;
                    n++;
                    break;
                }
            }
            n++;
        }
        if(fmt_status == 0){
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, std::string(), 0));
                nstr.clear();
            }
            str = m_pattern.substr(i+1, n-i-1);
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } else if(fmt_status == 1) {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
        } else if(fmt_status == 2){
            if(!nstr.empty()){
                vec.push_back(std::make_tuple(nstr, "", 0));
                nstr.clear();
            }
            vec.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        }
    }

    if(!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, "", 0));
    } 
    //    "[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%F%T[%p]%T%f:%l%T%m%n"
    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
#define XX(str,C) \
            {#str,[](const std::string& fmt) {return FormatItem::ptr(new C(fmt));}}     
            XX(m,MessageFormatItem),
            XX(p,LevelFormatItem),
            XX(r,ElapseFormatItem),
            XX(t,ThreadIdFormatItem),
            XX(n,NewLineFormatItem),
            XX(d,DateTimeFormatItem),
            XX(f,FilenameFormatItem),
            XX(l,LineFormatItem),
            XX(T,TabFormatItem),
            XX(F,FiberIdFormatItem),
            XX(N,ThreadNameFormatItem)
#undef XX        
      }; 
    
    for(auto &i : vec) {
        if(std::get<2>(i) == 0) {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
            // std::cout<< "3" << std::endl;
        } else {
            auto it = s_format_items.find(std::get<0>(i));
            if ( it == s_format_items.end()) {
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                // std::cout<< "2" << std::endl;
                m_error = true;
            } else {
                m_items.push_back(it->second(std::get<1>(i)));
                // std::cout<< "1" << std::endl;
            }
            //  << "[" << std::get<0>(i) << "] - [" << std::get<1>(i) << "] - [" << std::get<2>(i) << "]" <<std::endl;
        }
    }
    // std::cout << m_items.size() << std::endl;
}

LoggerManager::LoggerManager(){
    m_root.reset(new Logger);
    if (m_root->m_appender.empty()){
        m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));
    }
    m_loggers[m_root->m_name] = m_root;
    init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name){
    Mutex::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }
    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}


struct LogAppenderDefine{
    std::string type = "StdoutAppender";
    LogLevel::Level level = LogLevel::INFO;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine &oth) const {
        return type==oth.type && level == oth.level && formatter==oth.formatter && file==oth.file;
    }
};

struct LogDefine{
    std::string name;
    LogLevel::Level level = LogLevel::INFO;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine &oth) const {
        return name==oth.name && level==oth.level && formatter==oth.formatter && appenders==oth.appenders;
    }

    bool operator<(const LogDefine &oth) const {
        return name < oth.name;
    }
};

template<>
class LexicalCast<std::string, std::set<LogDefine>>{
public:
    std::set<LogDefine> operator()(const std::string &v ){
        YAML::Node node = YAML::Load(v);
        std::set<LogDefine> vec;
        for(size_t i = 0; i < node.size() ;i++){
            YAML::Node n = node[i];
            if(!n["name"].IsDefined()){
                std::cout << "log config error, name is null, " << n << std::endl;
                continue;
            }
            LogDefine ld;
            ld.name = n["name"].as<std::string>();
            ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
            if(n["formatter"].IsDefined()) {
                ld.formatter = n["formatter"].as<std::string>();
            }
            if(n["appenders"].IsDefined()) {
                for(size_t x = 0;x<n["appenders"].size();++x){
                    auto a = n["appenders"][x];
                    if(!a["type"].IsDefined()){
                        std::cout << "log config error, appender type is null, " << n << std::endl;
                        continue;
                    }
                    std::string type = a["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if(type == "FileLogAppender") {
                        lad.type = "FileLogAppender";
                        if(!a["file"].IsDefined()){
                            std::cout << "log config error, fileappender file is null, " << n << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if(a["formatter"].IsDefined()){
                            lad.formatter = a["formatter"].as<std::string>();
                        } else {
                            lad.formatter = "[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%N%T%F%T[%p]%T%f:%l%T%m%n";
                        }
                    } else if (type == "StdoutLogAppender"){
                        lad.type = "StdoutLogAppender";
                        if(a["formatter"].IsDefined()){
                            lad.formatter = a["formatter"].as<std::string>();
                        } else {
                            lad.formatter = "[%d{%Y-%m-%d %H:%M:%S}]%T%t%T%N%T%F%T[%p]%T%f:%l%T%m%n";
                        }
                    } else{
                        std::cout << "log config error, appender type is invalid, " << n << std::endl;
                        continue;
                    }
                    ld.appenders.push_back(lad);
                }
            }
            vec.insert(ld);
        }
        return vec;
    }
};


template<>
class LexicalCast<std::set<LogDefine>, std::string>{
public:
    std::string operator()(const std::set<LogDefine> &v) {
        YAML::Node node;
        for(auto &i : v){
            YAML::Node n;
            n["name"] = i.name;
            if(i.level != LogLevel::UNKNOW) {
                n["level"] = LogLevel::ToString(i.level);
            }
            if(i.formatter.empty()){
                n["formatter"] = i.formatter;
            }
            for(auto &a : i.appenders) {
                YAML::Node na;
                if (a.type == "FileLogAppender") {
                    na["type"] = "FileLogAppender";
                    na["file"] = a.file;
                } else if (a.type == "StdoutLogAppender") {
                    na["type"] = "StdoutLogAppender";
                }
                if(a.level != LogLevel::UNKNOW) {
                    na["level"] = LogLevel::ToString(i.level);
                }
                if(!a.formatter.empty()) {
                    na["formatter"] = a.formatter;
                }

                n["appenders"].push_back(na);
            }
            node.push_back(n);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};


sylar::ConfigVar<std::set<LogDefine>>::ptr g_log_defines = Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter{
    LogIniter(){
        g_log_defines->addListener([](const std::set<LogDefine> &old_value,
                                                const std::set<LogDefine> &new_value) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
            for(auto &i : new_value) {
                auto it = old_value.find(i);
                sylar::Logger::ptr logger;

                if (it == old_value.end()) {
                    logger = SYLAR_LOG_NAME(i.name);
                    // logger.reset(new sylar::Logger(i.name));
                } else {
                    if(!(i == *it)){
                        logger = SYLAR_LOG_NAME(i.name);
                    } else {
                        continue;
                    }
                }
                logger->setLevel(i.level);
                if(!i.formatter.empty()){
                    logger->setFormatter(i.formatter);
                }
                logger->clearAppenders();
                
                for(auto &a : i.appenders) {
                    sylar::LogAppender::ptr ap;
                    if(a.type == "FileLogAppender") {
                        ap.reset(new FileLogAppender(i.name));
                    } else if(a.type == "StdoutLogAppender") {   
                        ap.reset(new StdoutLogAppender);
                    }
                    ap->setLevel(a.level);
                    if (a.formatter.empty()) {
                        LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                        if(!fmt->isError()){
                            ap->setFormatter(fmt);
                        } else {
                            std::cout << "log.name = " << i.name << "appender type = " << a.type << " formatter = " << a.formatter << "is invalid" << std::endl;
                        }
                    }
                    logger->addAppender(ap);
                }
            }

            for(auto &i : old_value) {
                auto it = new_value.find(i);
                if(it == new_value.end()) {
                    auto logger = SYLAR_LOG_NAME(i.name);
                    logger->setLevel((LogLevel::Level)100);  
                    logger->clearAppenders();
                }
            }
        });
    }
};

static LogIniter __log_init;


LogLevel::Level LogLevel::FromString(const std::string &str){
#define XX(level, v) \
    if (str == #v) { \
        return LogLevel::level; \
    }
    XX(DEBUG, debug);
    XX(INFO, info);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);

    XX(DEBUG, DEBUG);
    XX(INFO, INFO);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);
    return LogLevel::UNKNOW;
#undef XX
}

void LoggerManager::init(){

}
void Logger::clearAppenders(){
    Mutex::Lock lock(m_mutex);
    m_appender.clear();
}

void Logger::setFormatter(LogFormatter::ptr val){
    Mutex::Lock lock(m_mutex);
    m_formatter = val;

    for(auto &i : m_appender){
        Mutex::Lock ll(i->m_mutex);
        if(!i->m_hasFormatter) {
            i->m_formatter = m_formatter;
        }
    }
}
void Logger::setFormatter(const std::string &val){
    sylar::LogFormatter::ptr new_val(new sylar::LogFormatter(val));
    if(new_val->isError()){
        std::cout << "LOGGER SET FORMATTER ERROR" << std::endl;
        return;
    }
    // m_formatter = new_val;
    setFormatter(new_val);
}
LogFormatter::ptr Logger::getFormatter() {
    Mutex::Lock lock(m_mutex);
    return m_formatter;
}

std::string LoggerManager::toYamlString(){
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    for(auto &i : m_loggers){
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

std::string Logger::toYamlString(){
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if(m_level != LogLevel::UNKNOW) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    for(auto &i : m_appender){
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

std::string FileLogAppender::toYamlString(){
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if(m_level != LogLevel::UNKNOW){
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

std::string StdoutLogAppender::toYamlString(){
    Mutex::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if(m_level != LogLevel::UNKNOW){
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter){
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}
}

