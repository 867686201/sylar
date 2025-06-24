#include "log.h"

namespace sylar{


// Logger
Logger::Logger(const std::string& name)
    : m_name(name)
{

}

void Logger::log(LogLevel level, LogEventPtr event)
{
    if(level > m_level)
    {
        for(auto& i : m_appenders)
        {
            i->log(level, event);
        }
    }
}

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

}