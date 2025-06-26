#include "log.h"
#include <iostream>
#include <iomanip>

namespace sylar {


    std::string toString(LogLevel level)
    {
        // 有 return 不需要 break
        switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOW";
        }
    }

    LogLevel parseLogLevel(const std::string& levelStr)
    {
        if (levelStr == "DEBUG") return LogLevel::DEBUG;
        if (levelStr == "INFO")  return LogLevel::INFO;
        if (levelStr == "WARN")  return LogLevel::WARN;
        if (levelStr == "ERROR") return LogLevel::ERROR;
        if (levelStr == "FATAL") return LogLevel::FATAL;

        // 可以选择抛异常或返回 UNKNOW
        return LogLevel::UNKNOW;
    }

    // Logger
    Logger::Logger(const std::string& name)
        : m_name(name), m_level(LogLevel::DEBUG) //  添加默认级别
    {
        // 如果没有设置formatter，使用默认的
        if (!m_formatter) {
            m_formatter = std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S} [%p] %c: %m%n");
        }
    }
    void Logger::addAppender(LogAppenderPtr appender)
    {
        m_appenders.push_back(appender);
    }

    void Logger::delAppender(LogAppenderPtr appender)
    {
        for (auto it = m_appenders.begin(); it != m_appenders.end(); it++)
        {
            if (*it == appender)
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
        if (level >= m_level)
        {
            for (auto& i : m_appenders)
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

    // formatter可能为空
    void StdoutLogAppender::log(LogLevel level, LogEventPtr event)
    {
        if (m_formatter) { // 添加空指针检查
            std::cout << m_formatter->format(event);
        }
        else {
            std::cout << event->getContent() << std::endl; // 默认输出
        }
    }

    FileLogAppender::FileLogAppender(const std::string& filename)
        : m_filename(filename)
    {

    }

    // formatter可能为空
    void FileLogAppender::log(LogLevel level, LogEventPtr event)
    {
        if (level >= m_level)
        {
            if (!m_filestream.is_open()) { // 确保文件已打开
                reopen();
            }
            if (m_formatter && m_filestream.is_open()) {
                m_filestream << m_formatter->format(event);
                m_filestream.flush(); // 立即刷新到磁盘
            }
        }
    }

    bool FileLogAppender::reopen()
    {
        if (m_filestream.is_open()) // 使用is_open()更准确
        {
            m_filestream.close();
        }
        m_filestream.open(m_filename, std::ios::app); // 使用追加模式
        return m_filestream.is_open(); //  返回true表示成功，false表示失败
    }

    class FormatterFactory
    {
    public:
        static FormatItemPtr createFormatItem(const Spec& spec)
        {
            switch (spec.convertType)
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



    std::ostream& LogFormatter::format(std::ostream& os, LogEventPtr event)
    {
        for (auto& item : m_items)  // 引用防止复制
        {
            item->format(os, event);
        }
        return os;
    }

    std::string LogFormatter::format(LogEventPtr event)
    {
        std::stringstream ss;
        for (auto& item : m_items)  // 引用防止复制
        {
            item->format(ss, event);
        }
        return ss.str();
    }

    void LogFormatter::init()
    {

        while (!eof()) // 没有读取完pattern
        {
            if (peek() != '%') // 处理字符串
            {
                Spec spec;
                size_t start = m_pos;
                while (!eof() && peek() != '%') m_pos++;
                // abc%d%%abc 循环结束时 start = 0, m_pos = 3;
                std::string ss = m_pattern.substr(start, m_pos - start);
                spec.optionalPara = ss;
                m_items.push_back(FormatterFactory::createFormatItem(spec));

            }
            else if (peek() == '%') // 处理 %
            {
                if (m_pos + 1 < m_pattern.size() && m_pattern[m_pos + 1] == '%') // 转义符 %
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
        if (peek() == '-')  // 有 `-` 则是左对齐
        {
            spec.leftAlign = true;
            get(); // 跳过 -
        }
        if (peek() == '.') // 有 `.` 则是最大宽度
        {
            isMaxWidth = true;
            get();
        }
        if (std::isdigit(peek())) // 找数字
        {
            int num;
            size_t start = m_pos;
            while (!eof() && std::isdigit(peek())) m_pos++;  // 已经到了非数字部分
            num = std::stoi(m_pattern.substr(start, m_pos - start));
            if (isMaxWidth)
            {
                spec.maxWidth = num;
            }
            else
            {
                spec.minWidth = num;
            }
        }
        if (std::isalpha(peek()))
        {
            spec.convertType = get(); // 获取的同时推进
        }
        if (peek() == '{')
        {
            get();
            size_t start = m_pos;
            while (!eof() && peek() != '}') m_pos++;  // 已经到了 } 
            spec.optionalPara = m_pattern.substr(start, m_pos - start); // substr 是 [a, b) 
            if (peek() == '}') get(); // 跳过 '}'
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
            }
            else {
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
        // 将事件的时间戳从毫秒转换为秒，因为 std::time_t 通常以秒为单位
        std::time_t time = static_cast<std::time_t>(event->getTime() / 1000);
        // 将时间戳转换为本地时间结构体 tm
        struct std::tm* tm_info = std::localtime(&time);
        if (!m_optionalPara.empty())  // 输入了新的时间格式, 则替换掉默认的格式
        {
            m_dateFormate = m_optionalPara;
        }
        // 使用 strftime 格式化时间到缓冲区 buffer（最大长度 256）
        char buffer[256];
        std::strftime(buffer, sizeof(buffer), m_dateFormate.c_str(), tm_info);

        formatOutput(os, std::string(buffer));
    }

    void LevelFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, toString(event->getLevel()));
    }

    void LoggerNameFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, event->getLogger()->getName());
        
    }

    void MessageFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, event->getContent());
    }


    void NewLineFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        os << std::endl;
    }

    void FileFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        const char* filename = event->getFile();
        formatOutput(os, filename ? std::string(filename) : "");
    }

    void LineFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, std::to_string(event->getLine()));
    }

    void ThreadIdFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, std::to_string(event->getThreadId()));
    }

    void FiberIdFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, std::to_string(event->getFiberId()));
    }

    void ElapseFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, std::to_string(event->getElapse()));
    }

    void ThreadNameFormatItem::format(std::ostream& os, LogEventPtr event)
    {
        formatOutput(os, event->getThreadName());
    }

}