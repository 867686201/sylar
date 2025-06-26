#include "log.h"
#include <iostream>
#include <iomanip>

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

class FormatterFactory 
{
public:
    static FormatItemPtr createFormatItem(const Spec& spec)
    {
        switch(spec.convertType)
        {
            case 'd': return std::make_shared<DateFormatItem>(spec);
            case 'p': return std::make_shared<LevelFormatItem>(spec);
            case 'c': return std::make_shared<LoggerNameFormatItem>(spec);
            case 'm': return std::make_shared<MessageFormatItem>(spec);
            case 'n': return std::make_shared<NewLineFormatItem>(spec);
            case 'f': return std::make_shared<FileFormatItem>(spec);      // 文件名
            case 'l': return std::make_shared<LineFormatItem>(spec);      // 行号
            case 't': return std::make_shared<ThreadIdFormatItem>(spec);  // 线程ID
            case 'F': return std::make_shared<FiberIdFormatItem>(spec);   // 协程ID
            case 'r': return std::make_shared<ElapseFormatItem>(spec);    // 运行时间
            case 'N': return std::make_shared<ThreadNameFormatItem>(spec);    // 线程名称
            default: return std::make_shared<StringFormatItem>(spec);
        }
    }
};

void LogFormatter::init()
{
    
    while(!eof()) // 没有读取完pattern
    {
        if(peek() != '%') // 处理字符串
        {
            Spec spec;
            size_t start =  m_pos;
            while(!eof() && peek() != '%') m_pos++;
            // abc%d%%abc 循环结束时 start = 0, m_pos = 3;
            std::string ss = m_pattern.substr(start, m_pos - start);
            spec.optionalPara = ss;
            m_items.push_back(FormatterFactory::createFormatItem(spec));

        }
        else if(peek() == '%') // 处理 %
        {
            if(m_pos + 1 < m_pattern.size() && m_pattern[m_pos + 1] == '%') // 转义符 %
            {
                m_items.push_back(std::make_shared<StringFormatItem>("%"));
                m_pos += 2;                                                 // 跳过 %%
            }
            else // 处理转换
                convert();
        }
    }
}

void LogFormatter::convert()
{
    bool isMaxWidth = false;
    Spec spec; // 一次函数调用只处理一个 spec
    get(); // 跳过 %
    if(peek() == '-')  // 有 `-` 则是左对齐
    {
        spec.leftAlign = true;
        get(); // 跳过 -
    }
    if(peek() == '.') // 有 `.` 则是最大宽度
    {
        isMaxWidth = true;
        get();
    }
    if(std::isdigit(peek())) // 找数字
    {
        int num;
        size_t start = m_pos;
        while(!eof() && std::isdigit(peek())) m_pos++;  // 已经到了非数字部分
        num = std::stoi(m_pattern.substr(start, m_pos-start));
        if(isMaxWidth)
        {
            spec.maxWidth = num;
        }
        else 
        {
            spec.minWidth = num;
        }
    }
    if(std::isalpha(peek()))
    {
        spec.convertType = get(); // 获取的同时推进
    }
    if(peek() == '{')
    {
        get();
        size_t start = m_pos;
        while(!eof() && peek() != '}') m_pos++;  // 已经到了 } 
        spec.optionalPara = m_pattern.substr(start, m_pos - start); // substr 是 [a, b) 
        if(peek() == '}') get(); // 跳过 '}'
    }
    m_items.push_back(FormatterFactory::createFormatItem(spec));

}

// 传入输出流和 string, 根据 FormatItem 内部的格式, 输出对应的 os
// 调用时 从 os << str 变为 formatOutput(os, str)
void FormatItem::formatOutput(std::ostream& os, const std::string& content) const
{
    std::string finalContent = content;
    // 应用最大宽度限制（截断）
    if (m_maxWidth > 0 && finalContent.length() > static_cast<size_t>(m_maxWidth)) {
        finalContent = finalContent.substr(0, m_maxWidth);
    }
    
    // 应用最小宽度和对齐方式
    if (m_minWidth > 0) {
        if (m_leftAlign) {
            os << std::left << std::setw(m_minWidth);   // iomanip
        } else {
            os << std::right << std::setw(m_minWidth);
        }
    }
    
    os << finalContent;
}

void StringFormatItem::format(std::ostream& os, LogEventPtr event)
{
    formatOutput(os, m_str);
}

void DateFormatItem::format(std::ostream& os, LogEventPtr event)
{
    // os << m_str;
}

void LevelFormatItem::format(std::ostream& os, LogEventPtr event)
{
    // os << m_str;
}

void LoggerNameFormatItem::format(std::ostream& os, LogEventPtr event)
{
    // os << m_str;
}

void MessageFormatItem::format(std::ostream& os, LogEventPtr event)
{
    // os << m_str;
}


}