#include "log.h"
#include <iostream>

namespace sylar{


// Logger
Logger::Logger(const std::string& name)
    : m_name(name)
{

}
void Logger::addAppender(LogAppenderPtr appender)
{
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppenderPtr appender)
{
    for(auto it = m_appenders.begin(); it != m_appenders.end(); it++)
    {
        if(*it == appender)
        {
            m_appenders.erase(it);
            break;
        }
    }
}

// 用于记录日志事件，判断日志是否需要输出，并通过接收器和格式化器进行输出
void Logger::log(LogLevel level, LogEventPtr event)
{
    // 当等级大于 `m_level` 时输出，即**更严重**时输出
    if(level >= m_level)
    {
        for(auto& i : m_appenders)
        {
            i->log(level, event);
        }
    }
}

// 对 `log` 的封装，简化不同日志级别的记录
void Logger::debug(LogEventPtr event)
{
    log(LogLevel::DEBUG, event);
}
void Logger::info(LogEventPtr event)
{
    log(LogLevel::INFO, event);
}
void Logger::warn(LogEventPtr event)
{
    log(LogLevel::WARN, event);
}
void Logger::error(LogEventPtr event)
{
    log(LogLevel::ERROR, event);
}
void Logger::fatal(LogEventPtr event)
{
    log(LogLevel::FATAL, event);
}

void StdoutLogAppender::log(LogLevel level, LogEventPtr event) 
{
    if(level >= m_level)
    {
        std::cout << m_formatter->format(event);
    }
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : m_filename(filename)
{

}

void FileLogAppender::log(LogLevel level, LogEventPtr event) 
{
    if(level >= m_level)
    {
        m_filestream << m_formatter->format(event);
    }
}

bool FileLogAppender::reopen()
{
    if(m_filestream)
    {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !m_filestream;
}

}