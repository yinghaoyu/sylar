#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include "singleton.h"
#include "thread.h"
#include "util.h"

#include <stdarg.h>
#include <stdint.h>
#include <chrono>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#define SYLAR_LOG_LEVEL(logger, level)                               \
  if (logger->getLevel() <= level)                                   \
  sylar::LogEventWrap(std::make_shared<sylar::LogEvent>(             \
                          logger, level, __FILE__, __LINE__, 0,      \
                          sylar::GetThreadId(), sylar::GetFiberId(), \
                          std::chrono::system_clock::now(),          \
                          sylar::Thread::GetName()))                 \
      .getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...)                 \
  if (logger->getLevel() <= level)                                   \
  sylar::LogEventWrap(std::make_shared<sylar::LogEvent>(             \
                          logger, level, __FILE__, __LINE__, 0,      \
                          sylar::GetThreadId(), sylar::GetFiberId(), \
                          std::chrono::system_clock::now(),          \
                          sylar::Thread::GetName()))                 \
      .getEvent()                                                    \
      ->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) \
  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) \
  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) \
  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) \
  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) \
  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar {

class Logger;
class LoggerManager;

// 日志级别
class LogLevel {
 public:
  enum Level {
    UNKNOW = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5
  };

  static const char* ToString(LogLevel::Level level);
  static LogLevel::Level FromString(const std::string& str);
};

// 日志事件
class LogEvent {
 public:
  typedef std::shared_ptr<LogEvent> ptr;
  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char* file, size_t m_line, size_t elapse, size_t thread_id,
           size_t fiber_id, std::chrono::system_clock::time_point time,
           const std::string& thread_name);

  const char* getFile() const { return m_file; }
  size_t getLine() const { return m_line; }
  size_t getElapse() const { return m_elapse; }
  size_t getThreadId() const { return m_threadId; }
  size_t getFiberId() const { return m_fiberId; }
  std::chrono::system_clock::time_point getTime() const { return m_time; }
  const std::string& getThreadName() const { return m_threadName; }
  std::string getContent() const { return m_ss.str(); }
  std::shared_ptr<Logger> getLogger() const { return m_logger; }
  LogLevel::Level getLevel() const { return m_level; }

  std::stringstream& getSS() { return m_ss; }
  void format(const char* fmt, ...);
  void format(const char* fmt, va_list al);

 private:
  const char* m_file = nullptr;                  // 文件名
  size_t m_line = 0;                             // 行号
  size_t m_elapse = 0;                           // 程序启动开始到现在的毫秒数
  size_t m_threadId = 0;                         // 线程id
  size_t m_fiberId = 0;                          // 协程id
  std::chrono::system_clock::time_point m_time;  // 时间戳
  std::string m_threadName;
  std::stringstream m_ss;

  std::shared_ptr<Logger> m_logger;
  LogLevel::Level m_level;
};

class LogEventWrap {
 public:
  LogEventWrap(LogEvent::ptr e);
  ~LogEventWrap();
  LogEvent::ptr getEvent() const { return m_event; }
  std::stringstream& getSS();

 private:
  LogEvent::ptr m_event;
};

// 日志格式器
class LogFormatter {
 public:
  typedef std::shared_ptr<LogFormatter> ptr;
  LogFormatter(const std::string& pattern);

  //%t    %thread_id %m%n
  std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                     LogEvent::ptr event);
  std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger,
                       LogLevel::Level level, LogEvent::ptr event);

 public:
  class FormatItem {
   public:
    typedef std::shared_ptr<FormatItem> ptr;
    virtual ~FormatItem() {}
    virtual void format(std::ostream& os, std::shared_ptr<Logger> logger,
                        LogLevel::Level level, LogEvent::ptr event) = 0;
  };

  bool isError() const { return m_error; }
  const std::string getPattern() const { return m_pattern; }

 private:
  void compile();

  std::string m_pattern;
  std::vector<FormatItem::ptr> m_items;
  bool m_error = false;
};

// 日志输出地
class LogAppender {
  friend class Logger;

 public:
  typedef std::shared_ptr<LogAppender> ptr;
  typedef Spinlock MutexType;
  virtual ~LogAppender() {}

  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   LogEvent::ptr event) = 0;
  virtual std::string toYamlString() = 0;

  void setFormatter(LogFormatter::ptr val);
  LogFormatter::ptr getFormatter();

  LogLevel::Level getLevel() const { return m_level; }
  void setLevel(LogLevel::Level val) { m_level = val; }

  virtual bool reopen() { return true; }

 protected:
  LogLevel::Level m_level = LogLevel::DEBUG;
  bool m_hasFormatter = false;
  MutexType m_mutex;
  LogFormatter::ptr m_formatter;
};

// 日志器
class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;

 public:
  typedef std::shared_ptr<Logger> ptr;
  typedef RWMutex RWMutexType;

  Logger(const std::string& name = "root");
  void log(LogLevel::Level level, LogEvent::ptr event);

  void debug(LogEvent::ptr event);
  void info(LogEvent::ptr event);
  void warn(LogEvent::ptr event);
  void error(LogEvent::ptr event);
  void fatal(LogEvent::ptr event);

  void addAppender(LogAppender::ptr appender);
  void delAppender(LogAppender::ptr appender);
  void clearAppenders();
  LogLevel::Level getLevel() const { return m_level; }
  void setLevel(LogLevel::Level val) { m_level = val; }

  const std::string& getName() const { return m_name; }

  void setFormatter(LogFormatter::ptr val);
  void setFormatter(const std::string& val);
  LogFormatter::ptr getFormatter();

  std::string toYamlString();

  bool reopen();

 private:
  std::string m_name;       // 日志名称
  LogLevel::Level m_level;  // 日志级别
  RWMutexType m_mutex;
  std::list<LogAppender::ptr> m_appenders;  // Appender集合
  LogFormatter::ptr m_formatter;
  Logger::ptr m_root;
};

// 输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<StdoutLogAppender> ptr;
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
  std::string toYamlString() override;
};

class LoadBalance;
class LogserverAppender : public LogAppender {
 public:
  typedef std::shared_ptr<LogserverAppender> ptr;
  LogserverAppender(const std::string& topic);
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
  std::string toYamlString() override;

 private:
  std::shared_ptr<LoadBalance> m_lb;
  std::string m_topic;
  std::string m_key;
};

// 定义输出到文件的Appender
class FileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<FileLogAppender> ptr;
  FileLogAppender(const std::string& filename);
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
  std::string toYamlString() override;

  // 重新打开文件，文件打开成功返回true
  bool reopen() override;

 private:
  std::string m_filename;
  std::ofstream m_filestream;
  std::chrono::system_clock::time_point m_lastTime;
};

class LoggerManager {
 public:
  typedef RWMutex RWMutexType;
  LoggerManager();
  Logger::ptr getLogger(const std::string& name);

  void init();
  Logger::ptr getRoot() const { return m_root; }

  std::string toYamlString();

  bool reopen();

 private:
  RWMutexType m_mutex;
  std::map<std::string, Logger::ptr> m_loggers;
  Logger::ptr m_root;
};

typedef sylar::Singleton<LoggerManager> LoggerMgr;

}  // namespace sylar

#endif
