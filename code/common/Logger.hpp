// 单例模式、策略模式
// 实现线程安全的日志对象
#pragma once
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include<pthread.h>

class Mutex
{
public:
    Mutex()
    {
        pthread_mutex_init(&_lock, nullptr);
    }
    ~Mutex()
    {
        pthread_mutex_destroy(&_lock);
    }
    void Lock()
    {
        pthread_mutex_lock(&_lock);
    }
    void Unlock()
    {
        pthread_mutex_unlock(&_lock);
    }
    pthread_mutex_t* Ptr()
    {
        return &_lock;
    }
private:
    pthread_mutex_t _lock;
};

// RAII风格用法
class LockGuard
{
public:
    LockGuard(Mutex& lock)
        :_lockref(lock)
    {
        _lockref.Lock();
    }
    ~LockGuard()
    {
        _lockref.Unlock();
    }
private:
    Mutex& _lockref;
};

/*
后续使用我们自己封装的锁时，可以写成：
{
    LockGuard lock(mutex);
    // ...
    // 临界区
}
{}划定作用域, 利用LockGuard自动调用构造析构完成加锁和解锁
*/


// 日志等级
enum class LogLevel
{
    INFO,
    WARNING,
    ERROR,
    FATAL,
};
std::string LogLevel2String(LogLevel level)
{
    switch (level)
    {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

// 刷新策略的基类
class LogStrategy
{
public:
    virtual ~LogStrategy()
    {
    }

    // 写日志的具体方法，每种策略中必须重写该函数
    virtual void SynLog(const std::string& message) = 0;
};

// 显示器打印日志策略
class ConsoleStrategy : public LogStrategy
{
public:
    void SynLog(const std::string& message) override
    {
        LockGuard lg(_mutex);
        std::cout << message << std::endl;
    }

private:
    Mutex _mutex;
};

// 文件写入日志策略
const std::string default_logpath = "./log";
const std::string default_logfilename = "log.txt";
class FileStrategy : public LogStrategy
{
public:
    FileStrategy(const std::string& logpath = default_logpath, const std::string& logfilename = default_logfilename)
        : _logpath(logpath), _logfilename(logfilename)
    {
        // 如果传入的目录和文件不存在则创建

        LockGuard lg(_mutex);

        // C++17引入的std::filesystem库用法
        if (std::filesystem::exists(_logpath))
            return;
        try
        {
            std::filesystem::create_directories(_logpath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    void SynLog(const std::string& message) override
    {
        LockGuard lg(_mutex);

        if (_logpath != "" && _logpath.back() != '/')
        {
            _logpath += '/';
        }
        std::string targetlog = _logpath + _logfilename;
        std::ofstream out(targetlog, std::ios::app);
        if (!out.is_open())
        {
            std::cerr << "open " << targetlog << " fail!" << std::endl;
            return;
        }
        out << message << '\n';
        out.close();
    }

private:
    std::string _logpath;     // 日志文件所在目录
    std::string _logfilename; // 日志文件名
    Mutex _mutex;
};

// 按等级写入不同文件策略
class FileLevelStrategy : public LogStrategy
{
public:
    FileLevelStrategy(const std::string& logpath = default_logpath) : _logpath(logpath)
    {
        // 如果传入的目录不存在则创建

        LockGuard lg(_mutex);

        // C++17引入的std::filesystem库用法
        if (std::filesystem::exists(_logpath))
            return;
        try
        {
            std::filesystem::create_directories(_logpath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    void SynLog(const std::string& message) override
    {
        LockGuard lg(_mutex);

        if (_logpath != "" && _logpath.back() != '/')
        {
            _logpath += '/';
        }

        size_t pos1 = message.find('[', 1);
        size_t pos2 = message.find(']', pos1);
        if(pos1 != std::string::npos && pos2 != std::string::npos)
        {
            std::string level = message.substr(pos1+1, pos2-pos1-1);
            _logfilename = "log." + level + ".txt";
        }
       
        std::string targetlog = _logpath + _logfilename;
        std::ofstream out(targetlog, std::ios::app);
        if (!out.is_open())
        {
            std::cerr << "open " << targetlog << " fail!" << std::endl;
            return;
        }
        out << message << '\n';
        out.close();
    }

private:
    std::string _logpath;
    std::string _logfilename;
    Mutex _mutex;
};

// 获取当前时间方法
std::string GetTime()
{
    struct timeval cur_time;
    gettimeofday(&cur_time, nullptr);
    struct tm struct_time;
    localtime_r(&(cur_time.tv_sec), &struct_time);
    char timestr[128];
    snprintf(timestr, sizeof timestr, "%04d-%02d-%02d %02d:%02d:%02d", struct_time.tm_year + 1900,
             struct_time.tm_mon + 1, struct_time.tm_mday, struct_time.tm_hour, struct_time.tm_min, struct_time.tm_sec);
    return timestr;
}

// 日志类需要完成: 1.生成日志信息 2.根据不同的策略进行刷新
class Logger
{
private:
    std::unique_ptr<LogStrategy> _strategy; // 刷新策略

public:
    // 内部类，描述一条完整的日志信息:
    // [时间][日志等级][进程id][文件名][代码行号] - 内容信息
    class LogMessage
    {
    public:
        LogMessage(LogLevel level, int line, std::string& filename, Logger& logger)
            : _cur_time(GetTime()), _level(level), _pid(getpid()), _filename(filename), _line(line), _logger(logger)
        {
            std::stringstream ss;
            ss << '[' << _cur_time << ']' << '[' << LogLevel2String(_level) << ']' << '[' << _pid << ']' << '['
               << _filename << ']' << '[' << _line << ']' << " - ";
            _loginfo = ss.str();
        }

        // 读取用户输入的内容信息
        template <class T> LogMessage& operator<<(const T& info)
        {
            std::stringstream ss;
            ss << info;
            _loginfo += ss.str();
            return *this;
        }

        // RAII自动刷新
        ~LogMessage()
        {
            _logger._strategy->SynLog(_loginfo);
        }

    private:
        std::string _cur_time;
        LogLevel _level;
        pid_t _pid;
        std::string _filename;
        int _line;
        std::string _loginfo;

        Logger& _logger;
    };

public:
    Logger()
    {
        // 默认使用显示器打印策略
        _strategy = std::make_unique<ConsoleStrategy>();
    }

    void UseConsoleStrategy()
    {
        _strategy = std::make_unique<ConsoleStrategy>();
    }

    void UseFileStrategy()
    {
        _strategy = std::make_unique<FileStrategy>();
    }

    void UseFileLevelStrategy()
    {
        _strategy = std::make_unique<FileLevelStrategy>();
    }

    LogMessage operator()(LogLevel level, std::string filename, int line)
    {
        return LogMessage(level, line, filename, *this);
    }
};

Logger logger;

#define USE_CONSOLE_LOG_STRATEGY() logger.UseConsoleStrategy();
#define USE_FILE_LOG_STRATEGY() logger.UseFileStrategy();
#define USE_FILE_LEVEL_LOG_STRATEGY() logger.UseFileLevelStrategy();

#define LOG(level) logger(level, __FILE__, __LINE__)

// 至此，我们就可以用 "LOG(level) << 日志信息" 的方式进行日志写入刷新了
