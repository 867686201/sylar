#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <fstream>
#include <list>
#include <vector>

namespace sylar
{


    // 前向声明
    class LogEvent;
    class Logger;
    class LogFormatter;
    class LogAppender;
    class FileLogAppender;
    class StdoutLogAppender;
    class FormatItem;


    using LogEventPtr = std::shared_ptr<LogEvent>;
    using LoggerPtr = std::shared_ptr<Logger>;
    using LogFormatterPtr = std::shared_ptr<LogFormatter>;
    using LogAppenderPtr = std::shared_ptr<LogAppender>;
    using FileLogAppenderPtr = std::shared_ptr<FileLogAppender>;
    using StdoutLogAppenderPtr = std::shared_ptr<StdoutLogAppender>;
    using FormatItemPtr = std::shared_ptr<FormatItem>;

    enum class LogLevel
    {
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    std::string toString(LogLevel level);       // logLevel 转 string
    LogLevel parseLogLevel(const std::string& levelStr);

    class LogEvent
    {
    public:
        LogEvent(LoggerPtr logger, LogLevel level
            , const char* file, int32_t line, uint32_t elapse
            , uint32_t threadId, uint32_t fiberId, uint64_t time
            , const std::string& threadName)
            :
            m_logger(logger), m_level(level),
            m_file(file), m_line(line), m_elapse(elapse), m_threadId(threadId),
            m_fiberId(fiberId), m_time(time), m_threadName(threadName)
        {}

        // 添加getter方法
        const char* getFile() const { return m_file; }
        int32_t getLine() const { return m_line; }
        uint32_t getElapse() const { return m_elapse; }
        uint32_t getThreadId() const { return m_threadId; }
        uint32_t getFiberId() const { return m_fiberId; }
        uint64_t getTime() const { return m_time; }
        const std::string& getThreadName() const { return m_threadName; }
        LogLevel getLevel() const { return m_level; }
        LoggerPtr getLogger() const { return m_logger; }

        // 添加消息相关方法
        std::stringstream& getSS() { return m_ss; }
        const std::string getContent() const { return m_ss.str(); }

    private:
        const char* m_file = nullptr;  //文件名
        int32_t m_line = 0;            //行号
        uint32_t m_elapse = 0;         //程序启动开始到现在的毫秒数
        uint32_t m_threadId = 0;       //线程id
        uint32_t m_fiberId = 0;        //协程id
        uint64_t m_time = 0;           //时间戳
        std::string m_threadName;      //线程名称       读多写少, 内存连续，读取快
        std::stringstream m_ss;        //日志消息流     写多读少, 缓冲区优化，构建快

        LoggerPtr m_logger;
        LogLevel m_level;

    };

    // 日志器 
    class Logger
    {
    public:
        // const 确保参数不能被改变, & 表示引用, 防止拷贝
        Logger(const std::string& name = "root");
        void log(LogLevel level, LogEventPtr event);

        void debug(LogEventPtr event);
        void info(LogEventPtr event);
        void warn(LogEventPtr event);
        void error(LogEventPtr event);
        void fatal(LogEventPtr event);

        void addAppender(LogAppenderPtr appender);
        void delAppender(LogAppenderPtr appender);

        LogLevel getLevel() const { return m_level; }
        void setLevel(LogLevel val) { m_level = val; }

        // 返回引用, 且不能再外部被修改
        const std::string& getName() const { return m_name; }
    private:
        std::string m_name;
        LogLevel m_level;
        std::list<LogAppenderPtr> m_appenders;      // appender 集合
        LogFormatterPtr m_formatter;

    };


    // 日志接收器  抽象基类 定义接口名称
    class LogAppender
    {
    public:
        // 抽象基类的构造函数不能是虚函数
        LogAppender() = default;
        // 抽象基类的析构函数必须是虚函数, 为了父类指针调用子类的析构函数来析构子类对象
        virtual ~LogAppender() {};

        // 没有实现的纯虚函数, 使得这个类叫做抽象基类, 必须实现该方法类才能实例化       
        virtual void log(LogLevel level, LogEventPtr event) = 0;

        // 普通函数, 提供固定的实现         而虚函数提供默认的实现, 可以重写
        void setFormatter(LogFormatterPtr val) { m_formatter = val; }
        LogFormatterPtr getFormatter() const { return m_formatter; }

        LogLevel getLevel() const { return m_level; }
        void setLevel(LogLevel val) { m_level = val; }

    protected:
        LogLevel m_level = LogLevel::DEBUG;
        LogFormatterPtr m_formatter;
    };


    //输出到控制台的Appender
    class StdoutLogAppender : public LogAppender
    {
    public:
        void log(LogLevel level, LogEventPtr event) override;
    };

    //定义输出到文件的Appender
    class FileLogAppender : public LogAppender
    {
    public:
        FileLogAppender(const std::string& filename);
        void log(LogLevel level, LogEventPtr event) override;

        //重新打开文件，文件打开成功返回true
        bool reopen();
    private:
        std::string m_filename;
        std::ofstream m_filestream;
    };

    class LogFormatter
    {
    public:
        // 接收 pattern 进行构造
        LogFormatter(const std::string& pattern) : m_pattern(pattern) 
        {
            init();
        };
        std::ostream& format(std::ostream& os, LogEventPtr event);
        std::string format(LogEventPtr event);
        const std::string& getPattern() const { return m_pattern; }
    private:
        void init();                        // 解析模板函数

        void convert();

        bool eof() const { return m_pos >= m_pattern.size(); }
        char peek() const { return eof() ? '\0' : m_pattern[m_pos]; }
        char get() { return eof() ? '\0' : m_pattern[m_pos++]; }

        std::string m_pattern;              // 格式模版
        std::vector<FormatItemPtr> m_items; // 解析后的格式
        size_t m_pos = 0;                   // 下标
    };

    // 转换处理时结构体
    struct Spec
    {
        bool leftAlign = false;
        int minWidth = -1;
        int maxWidth = -1;
        char convertType = '\0';
        std::string optionalPara;
    };

    // 策略基类
    // 即定义抽象基类和多个具体实现
    // 然后可以用策略类来切换子类
    class FormatItem
    {
    public:
        FormatItem(const Spec& spec)
            : m_leftAlign(spec.leftAlign),
            m_minWidth(spec.minWidth),
            m_maxWidth(spec.maxWidth),
            m_optionalPara(spec.optionalPara)
        {};
        virtual ~FormatItem() = default;
        virtual void format(std::ostream& os, LogEventPtr event) = 0;
    protected:
        // 封装格式转换函数
        void formatOutput(std::ostream& os, const std::string& content) const;
        bool m_leftAlign;
        int m_minWidth;
        int m_maxWidth;
        std::string m_optionalPara;
    };

    // 解析时, 处理好数据后, 构造出对应的子类, 传入 m_items
    class StringFormatItem : public FormatItem
    {
    public:
        StringFormatItem(const std::string& s)
            : FormatItem(Spec{}), m_str(s) {}
        StringFormatItem(const Spec& spec)
            : FormatItem(spec), m_str(spec.optionalPara) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
        // 字面量字符串
        std::string m_str;
    };

    class DateFormatItem : public FormatItem
    {
    public:
        DateFormatItem(const Spec& spec)
            : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
        std::string m_dateFormate = "%Y-%m-%d %H:%M:%S";  // 默认日期格式
    };

    class LevelFormatItem : public FormatItem
    {
    public:
        LevelFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };

    class LoggerNameFormatItem : public FormatItem
    {
    public:
        LoggerNameFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };

    class MessageFormatItem : public FormatItem
    {
    public:
        MessageFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };

    class NewLineFormatItem : public FormatItem
    {
    public:
        NewLineFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };

    class FileFormatItem : public FormatItem
    {
    public:
        FileFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };
    class LineFormatItem : public FormatItem
    {
    public:
        LineFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };
    class ThreadIdFormatItem : public FormatItem
    {
    public:
        ThreadIdFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };
    class FiberIdFormatItem : public FormatItem
    {
    public:
        FiberIdFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };
    class ElapseFormatItem : public FormatItem
    {
    public:
        ElapseFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };

    class ThreadNameFormatItem : public FormatItem
    {
    public:
        ThreadNameFormatItem(const Spec& spec) : FormatItem(spec) {}
        void format(std::ostream& os, LogEventPtr event) override;
    private:
    };


}