#include <string.h>
#include <time.h>
#include <functional>
#include <iostream>
#include <map>

#include "application.h"
#include "config.h"
#include "env.h"
#include "log.h"
#include "macro.h"
#include "sylar/proto/logserver.pb.h"
#include "util.h"

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

const char* LogLevel::ToString(LogLevel::Level level) {
  switch (level) {
#define XX(name)       \
  case LogLevel::name: \
    return #name;      \
    break;

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
      return "UNKNOW";
  }
  return "UNKNOW";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v)        \
  if (str == #v) {          \
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

LogEventWrap::LogEventWrap(LogEvent::ptr e) : m_event(e) {}

LogEventWrap::~LogEventWrap() {
  m_event->getLogger()->log(m_event->getLevel(), m_event);
}

void LogEvent::format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);
  format(fmt, al);
  va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
  char* buf = nullptr;
  int len = vasprintf(&buf, fmt, al);
  if (len != -1) {
    m_ss << std::string(buf, len);
    free(buf);
  }
}

std::stringstream& LogEventWrap::getSS() {
  return m_event->getSS();
}

void LogAppender::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(m_mutex);
  m_formatter = val;
  if (m_formatter) {
    m_hasFormatter = true;
  } else {
    m_hasFormatter = false;
  }
}

LogFormatter::ptr LogAppender::getFormatter() {
  MutexType::Lock lock(m_mutex);
  return m_formatter;
}

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  MessageFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getContent();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  LevelFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << LogLevel::ToString(level);
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  ElapseFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getElapse();
  }
};

class NameFormatItem : public LogFormatter::FormatItem {
 public:
  NameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getLogger()->getName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getThreadId();
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  FiberIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFiberId();
  }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadNameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getThreadName();
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
      : m_format(format) {}

  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    static thread_local char buf[64];
    static thread_local std::chrono::seconds last_sec_{};

    std::chrono::system_clock::duration d = event->getTime().time_since_epoch();
    std::chrono::seconds s =
        std::chrono::duration_cast<std::chrono::seconds>(d);
    auto usec =
        std::chrono::duration_cast<std::chrono::microseconds>(d - s).count();
    char usbuf[8];
    snprintf(usbuf, sizeof(usbuf), ".%06ld", static_cast<long>(usec));
    if (last_sec_ == s) {
      os << buf << usbuf;
      return;
    }

    last_sec_ = s;
    auto time = std::chrono::system_clock::to_time_t(event->getTime());
    struct tm ltm;
    localtime_r(&time, &ltm);
    strftime(buf, sizeof(buf), m_format.c_str(), &ltm);
    os << buf << usbuf;
  }

 private:
  std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
 public:
  FilenameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFile();
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  LineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getLine();
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  NewLineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << std::endl;
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  StringFormatItem(const std::string& str) : m_string(str) {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << m_string;
  }

 private:
  std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  TabFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << "\t";
  }

 private:
  std::string m_string;
};

class SpaceFormatItem : public LogFormatter::FormatItem {
 public:
  SpaceFormatItem(const std::string& str = "") {}

  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr record) override {
    os << " ";
  }
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char* file, size_t line, size_t elapse,
                   size_t thread_id, size_t fiber_id,
                   std::chrono::system_clock::time_point time,
                   const std::string& thread_name)
    : m_file(file),
      m_line(line),
      m_elapse(elapse),
      m_threadId(thread_id),
      m_fiberId(fiber_id),
      m_time(time),
      m_threadName(thread_name),
      m_logger(logger),
      m_level(level) {}

Logger::Logger(const std::string& name)
    : m_name(name), m_level(LogLevel::DEBUG) {
  m_formatter = std::make_shared<LogFormatter>(
      "%d{%Y-%m-%d %H:%M:%S}%b%t%b%N%b%F%b%p%b%c%b[%f:%l]%b%m%n");
}

void Logger::setFormatter(LogFormatter::ptr val) {
  RWMutexType::WriteLock lock(m_mutex);
  m_formatter = val;
  for (auto& i : m_appenders) {
    LogAppender::MutexType::Lock ll(i->m_mutex);
    if (!i->m_hasFormatter) {
      i->m_formatter = m_formatter;
    }
  }
}

void Logger::setFormatter(const std::string& val) {
  sylar::LogFormatter::ptr new_val = std::make_shared<sylar::LogFormatter>(val);
  if (new_val->isError()) {
    std::cout << "Logger setFormatter name=" << m_name << " value=" << val
              << " invalid formatter" << std::endl;
    return;
  }
  setFormatter(new_val);
}

std::string Logger::toYamlString() {
  RWMutexType::ReadLock lock(m_mutex);
  YAML::Node node;
  node["name"] = m_name;
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }

  for (auto& i : m_appenders) {
    node["appenders"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

bool Logger::reopen() {
  RWMutexType::ReadLock lock(m_mutex);
  auto appends = m_appenders;
  lock.unlock();

  for (auto& i : appends) {
    i->reopen();
  }
  return true;
}

LogFormatter::ptr Logger::getFormatter() {
  RWMutexType::ReadLock lock(m_mutex);
  return m_formatter;
}

void Logger::addAppender(LogAppender::ptr appender) {
  RWMutexType::WriteLock lock(m_mutex);
  if (!appender->getFormatter()) {
    LogAppender::MutexType::Lock ll(appender->m_mutex);
    appender->m_formatter = m_formatter;
  }
  m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  RWMutexType::WriteLock lock(m_mutex);
  for (auto it = m_appenders.begin(); it != m_appenders.end(); ++it) {
    if (*it == appender) {
      m_appenders.erase(it);
      break;
    }
  }
}

void Logger::clearAppenders() {
  RWMutexType::WriteLock lock(m_mutex);
  m_appenders.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
  if (level >= m_level) {
    auto self = shared_from_this();
    RWMutexType::ReadLock lock(m_mutex);
    if (!m_appenders.empty()) {
      for (auto& i : m_appenders) {
        i->log(self, level, event);
      }
    } else if (m_root) {
      m_root->log(level, event);
    }
  }
}

void Logger::debug(LogEvent::ptr event) {
  log(LogLevel::DEBUG, event);
}

void Logger::info(LogEvent::ptr event) {
  log(LogLevel::INFO, event);
}

void Logger::warn(LogEvent::ptr event) {
  log(LogLevel::WARN, event);
}

void Logger::error(LogEvent::ptr event) {
  log(LogLevel::ERROR, event);
}

void Logger::fatal(LogEvent::ptr event) {
  log(LogLevel::FATAL, event);
}

FileLogAppender::FileLogAppender(const std::string& filename)
    : m_filename(filename) {
  reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                          LogEvent::ptr event) {
  if (level >= m_level) {
    auto now = event->getTime();
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_lastTime);
    if (duration.count() >= 3) {
      reopen();
      m_lastTime = now;
    }
    MutexType::Lock lock(m_mutex);
    if (!m_formatter->format(m_filestream, logger, level, event)) {
      std::cout << "error" << std::endl;
    }
  }
}

std::string FileLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = m_filename;
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_hasFormatter && m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

bool FileLogAppender::reopen() {
  MutexType::Lock lock(m_mutex);
  if (m_filestream) {
    m_filestream.close();
  }
  return FSUtil::OpenForWrite(m_filestream, m_filename, std::ios::app);
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                            LogLevel::Level level, LogEvent::ptr event) {
  if (level >= m_level) {
    MutexType::Lock lock(m_mutex);
    m_formatter->format(std::cout, logger, level, event);
  }
}

std::string StdoutLogAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_hasFormatter && m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogserverAppender::LogserverAppender(const std::string& topic)
    : m_topic(topic) {
  // auto app = sylar::Application::GetInstance()->getRockSDLoadBalance();
  // m_lb = app->get("logserver", "logserver", true);
}

void LogserverAppender::log(Logger::ptr logger, LogLevel::Level level,
                            LogEvent::ptr event) {
  if (level >= m_level) {
    std::stringstream ss;
    MutexType::Lock lock(m_mutex);
    m_formatter->format(ss, logger, level, event);
    lock.unlock();
    logserver::LogNotify nty;
    nty.set_body(ss.str());
    nty.set_topic(m_topic);
    nty.set_key(m_key);

    sylar::RockNotify::ptr rock_nty = std::make_shared<sylar::RockNotify>();
    rock_nty->setNotify(100);
    rock_nty->setAsPB(nty);

    for (size_t i = 0; i < 3; ++i) {
      if (!m_lb) {
        auto app = sylar::Application::GetInstance()->getRockSDLoadBalance();
        if (app) {
          m_lb = app->get("logserver", "logserver", true);
        }
      }
      if (!m_lb) {
        continue;
      }
      auto item = m_lb->get();
      if (!item) {
        continue;
      }
      auto conn = item->getStreamAs<sylar::RockStream>();
      if (!conn) {
        continue;
      }
      if (conn->sendMessage(rock_nty) <= 0) {
        continue;
      }
      return;
    }
  }
}

std::string LogserverAppender::toYamlString() {
  MutexType::Lock lock(m_mutex);
  YAML::Node node;
  node["type"] = "LogserverAppender";
  if (m_level != LogLevel::UNKNOW) {
    node["level"] = LogLevel::ToString(m_level);
  }
  if (m_hasFormatter && m_formatter) {
    node["formatter"] = m_formatter->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern) : m_pattern(pattern) {
  compile();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,
                                 LogLevel::Level level, LogEvent::ptr event) {
  std::stringstream ss;
  for (auto& i : m_items) {
    i->format(ss, logger, level, event);
  }
  return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs,
                                   std::shared_ptr<Logger> logger,
                                   LogLevel::Level level, LogEvent::ptr event) {
  for (auto& i : m_items) {
    i->format(ofs, logger, level, event);
  }
  return ofs;
}

enum STATUS {
  UNIT,
  STR,
};

void LogFormatter::compile() {
  size_t n = m_pattern.size();

  STATUS status = STR;

  std::string A;
  std::string B;

  std::vector<std::tuple<std::string, std::string, STATUS>> items;
  // 状态机
  for (size_t i = 0; i < n; ++i) {
    switch (status) {
      case STR: {
        if (m_pattern[i] == '%') {
          status = UNIT;
        } else {
          A = m_pattern[i];
          items.emplace_back(std::make_tuple(std::move(A), std::move(B), STR));
        }
      } break;
      case UNIT: {
        A = m_pattern[i];
        if (i + 1 < n && m_pattern[i + 1] == '{') {
          size_t pos = m_pattern.find('}', i + 1);
          if (pos != std::string::npos) {
            B = m_pattern.substr(i + 2, pos - i - 2);
            i = pos;
          };
        }
        items.emplace_back(std::make_tuple(std::move(A), std::move(B), UNIT));
        status = STR;
      } break;
      default:
        m_error = true;
        break;
    }
  }

#define XX(str, C)                                      \
  {                                                     \
    #str, [](const std::string& fmt) {                  \
      return FormatItem::ptr(std::make_shared<C>(fmt)); \
    }                                                   \
  }

  static std::map<std::string,
                  std::function<FormatItem::ptr(const std::string& str)>>
      s_format_units = {
          XX(m, MessageFormatItem),     // m:消息
          XX(p, LevelFormatItem),       // p:日志级别
          XX(r, ElapseFormatItem),      // r:累计毫秒数
          XX(c, NameFormatItem),        // c:日志名称
          XX(t, ThreadIdFormatItem),    // t:线程id
          XX(n, NewLineFormatItem),     // n:换行
          XX(d, DateTimeFormatItem),    // d:时间
          XX(f, FilenameFormatItem),    // f:文件名
          XX(l, LineFormatItem),        // l:行号
          XX(T, TabFormatItem),         // T:Tab
          XX(b, SpaceFormatItem),       // b:空格
          XX(F, FiberIdFormatItem),     // F:协程id
          XX(N, ThreadNameFormatItem),  // N:线程名称
      };

#undef XX

  for (auto& item : items) {
    if (std::get<2>(item) == STR) {
      m_items.emplace_back(
          std::make_shared<StringFormatItem>(std::get<0>(item)));
    } else {
      auto it = s_format_units.find(std::get<0>(item));
      if (it == s_format_units.end()) {
        m_items.emplace_back(std::make_shared<StringFormatItem>(
            "<error_format %" + std::get<0>(item) + ">"));
        m_error = true;
      } else {
        m_items.push_back(it->second(std::get<1>(item)));
      }
    }
  }
}

LoggerManager::LoggerManager() {
  m_root = std::make_shared<Logger>();
  m_root->addAppender(std::make_shared<StdoutLogAppender>());

  m_loggers[m_root->m_name] = m_root;

  init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
  {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
      return it->second;
    }
  }

  RWMutexType::WriteLock lock(m_mutex);
  auto it = m_loggers.find(name);
  if (it != m_loggers.end()) {
    return it->second;
  }

  Logger::ptr logger = std::make_shared<Logger>(name);
  logger->m_root = m_root;
  m_loggers[name] = logger;
  return logger;
}

struct LogAppenderDefine {
  int type = 0;  // 1 File, 2 Stdout, 3 Logserver
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::string file;

  bool operator==(const LogAppenderDefine& oth) const {
    return type == oth.type && level == oth.level &&
           formatter == oth.formatter && file == oth.file;
  }
};

struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::UNKNOW;
  std::string formatter;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine& oth) const {
    return name == oth.name && level == oth.level &&
           formatter == oth.formatter && appenders == appenders;
  }

  bool operator<(const LogDefine& oth) const { return name < oth.name; }

  bool isValid() const { return !name.empty(); }
};

template <>
class LexicalCast<std::string, LogDefine> {
 public:
  LogDefine operator()(const std::string& v) {
    YAML::Node n = YAML::Load(v);
    LogDefine ld;
    if (!n["name"].IsDefined()) {
      std::cout << "log config error: name is null, " << n << std::endl;
      throw std::logic_error("log config name is null");
    }
    ld.name = n["name"].as<std::string>();
    ld.level = LogLevel::FromString(
        n["level"].IsDefined() ? n["level"].as<std::string>() : "");
    if (n["formatter"].IsDefined()) {
      ld.formatter = n["formatter"].as<std::string>();
    }

    if (n["appenders"].IsDefined()) {
      // std::cout << "==" << ld.name << " = " << n["appenders"].size() <<
      // std::endl;
      for (size_t x = 0; x < n["appenders"].size(); ++x) {
        auto a = n["appenders"][x];
        if (!a["type"].IsDefined()) {
          std::cout << "log config error: appender type is null, " << a
                    << std::endl;
          continue;
        }
        std::string type = a["type"].as<std::string>();
        LogAppenderDefine lad;
        if (type == "FileLogAppender") {
          lad.type = 1;
          if (!a["file"].IsDefined()) {
            std::cout << "log config error: fileappender file is null, " << a
                      << std::endl;
            continue;
          }
          lad.file = a["file"].as<std::string>();
          if (a["formatter"].IsDefined()) {
            lad.formatter = a["formatter"].as<std::string>();
          }
        } else if (type == "StdoutLogAppender") {
          lad.type = 2;
          if (a["formatter"].IsDefined()) {
            lad.formatter = a["formatter"].as<std::string>();
          }
        } else if (type == "LogserverAppender") {
          lad.type = 3;
          if (!a["topic"].IsDefined()) {
            std::cout << "log config error: logserverappender topic is null, "
                      << a << std::endl;
            continue;
          }
          lad.file = a["topic"].as<std::string>();
          if (a["formatter"].IsDefined()) {
            lad.formatter = a["formatter"].as<std::string>();
          }
        } else {
          std::cout << "log config error: appender type is invalid, " << a
                    << std::endl;
          continue;
        }

        ld.appenders.push_back(lad);
      }
    }
    return ld;
  }
};

template <>
class LexicalCast<LogDefine, std::string> {
 public:
  std::string operator()(const LogDefine& i) {
    YAML::Node n;
    n["name"] = i.name;
    if (i.level != LogLevel::UNKNOW) {
      n["level"] = LogLevel::ToString(i.level);
    }
    if (!i.formatter.empty()) {
      n["formatter"] = i.formatter;
    }

    for (auto& a : i.appenders) {
      YAML::Node na;
      if (a.type == 1) {
        na["type"] = "FileLogAppender";
        na["file"] = a.file;
      } else if (a.type == 2) {
        na["type"] = "StdoutLogAppender";
      } else if (a.type == 3) {
        na["type"] = "LogserverAppender";
      }
      if (a.level != LogLevel::UNKNOW) {
        na["level"] = LogLevel::ToString(a.level);
      }

      if (!a.formatter.empty()) {
        na["formatter"] = a.formatter;
      }

      n["appenders"].push_back(na);
    }
    std::stringstream ss;
    ss << n;
    return ss.str();
  }
};

sylar::ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
    sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->addListener([](const std::set<LogDefine>& old_value,
                                  const std::set<LogDefine>& new_value) {
      SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
      for (auto& i : new_value) {
        auto it = old_value.find(i);
        sylar::Logger::ptr logger;
        if (it == old_value.end()) {
          // 新增logger
          logger = SYLAR_LOG_NAME(i.name);
        } else {
          if (!(i == *it)) {
            // 修改的logger
            logger = SYLAR_LOG_NAME(i.name);
          } else {
            continue;
          }
        }
        logger->setLevel(i.level);
        if (!i.formatter.empty()) {
          logger->setFormatter(i.formatter);
        }

        logger->clearAppenders();
        for (auto& a : i.appenders) {
          sylar::LogAppender::ptr ap;
          if (a.type == 1) {
            ap = std::make_shared<FileLogAppender>(a.file);
          } else if (a.type == 2) {
            if (!sylar::EnvMgr::GetInstance()->has("d")) {
              ap = std::make_shared<StdoutLogAppender>();
            } else {
              continue;
            }
          } else if (a.type == 3) {
            ap = std::make_shared<LogserverAppender>(a.file);
          }
          ap->setLevel(a.level);
          if (!a.formatter.empty()) {
            LogFormatter::ptr fmt = std::make_shared<LogFormatter>(a.formatter);
            if (!fmt->isError()) {
              ap->setFormatter(fmt);
            } else {
              std::cout << "log.name=" << i.name << " appender type=" << a.type
                        << " formatter=" << a.formatter << " is invalid"
                        << std::endl;
            }
          }
          logger->addAppender(ap);
        }
      }

      for (auto& i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          // 删除logger
          auto logger = SYLAR_LOG_NAME(i.name);
          logger->setLevel((LogLevel::Level)0);
          logger->clearAppenders();
        }
      }
    });
  }
};

bool LoggerManager::reopen() {
  RWMutexType::ReadLock lock(m_mutex);
  auto loggers = m_loggers;
  auto root = m_root;
  lock.unlock();

  root->reopen();
  for (auto& i : loggers) {
    i.second->reopen();
  }
  return true;
}

static LogIniter __log_init;

std::string LoggerManager::toYamlString() {
  RWMutexType::ReadLock lock(m_mutex);
  YAML::Node node;
  for (auto& i : m_loggers) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void LoggerManager::init() {}

}  // namespace sylar
