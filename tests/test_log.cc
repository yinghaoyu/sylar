#include "sylar/log.h"
#include "sylar/util.h"

#include <chrono>
#include <iostream>

void bench(const char* file, bool kLongLog) {
  auto logger = std::make_shared<sylar::Logger>("bench");

  sylar::LogFormatter::ptr formatter =
      std::make_shared<sylar::LogFormatter>("%m");  // only message

  sylar::FileLogAppender::ptr fileAppender =
      std::make_shared<sylar::FileLogAppender>(file);

  // sylar::ConsoleLogAppender::ptr consoleAppender =
  // std::make_shared<sylar::ConsoleLogAppender>();

  fileAppender->setFormatter(formatter);

  logger->addAppender(fileAppender);

  // consoleAppender->setFormatter(formatter);
  // logger->addAppender(consoleAppender);

  size_t n = 1000 * 1000;
  std::string content = "Hello 0123456789 abcdefghijklmnopqrstuvwxyz";
  std::string empty = " ";
  std::string longStr(3000, 'X');
  longStr += " ";
  size_t len = content.length() +
               (kLongLog ? longStr.length() : empty.length()) + sizeof(int);
  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < n; ++i) {
    SYLAR_LOG_DEBUG(logger) << content << (kLongLog ? longStr : empty) << i;
  }
  auto end = std::chrono::steady_clock::now();
  double seconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count() /
      1000.0;
  printf("%12s:%f seconds, %ld bytes, %10.2f msg/s, %.2f MiB/s\n", file,
         seconds, n * len, static_cast<double>(n) / seconds,
         static_cast<double>(n * len) / seconds / (1024 * 1024));
}

void test_logger() {
  auto logger = std::make_shared<sylar::Logger>("root");
  logger->addAppender(std::make_shared<sylar::StdoutLogAppender>());

  sylar::FileLogAppender::ptr fileAppender =
      std::make_shared<sylar::FileLogAppender>("./test.log");
  logger->addAppender(fileAppender);

  SYLAR_LOG_DEBUG(logger) << "debug";
  SYLAR_LOG_INFO(logger) << "info";
  SYLAR_LOG_WARN(logger) << "warn";
  SYLAR_LOG_ERROR(logger) << "error";
  SYLAR_LOG_FATAL(logger) << "fatal";

  SYLAR_LOG_FMT_DEBUG(logger, "fmt debug: %s", "I am a formatter string");
  SYLAR_LOG_FMT_INFO(logger, "fmt info: %s", "I am a formatter string");
  SYLAR_LOG_FMT_WARN(logger, "fmt warn: %s", "I am a formatter string");
  SYLAR_LOG_FMT_ERROR(logger, "fmt error: %s", "I am a formatter string");
  SYLAR_LOG_FMT_FATAL(logger, "fmt fatal: %s", "I am a formatter string");

  logger->setLevel(sylar::LogLevel::ERROR);
  SYLAR_LOG_INFO(logger) << "this message never sink";

  auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
  SYLAR_LOG_INFO(l) << "xx logger";

  auto root = SYLAR_LOG_ROOT();
  SYLAR_LOG_INFO(root) << "root logger";
}

int main() {
  test_logger();
  bench("/dev/null", false);
  bench("/tmp/log", false);
  bench("./bench.log", false);
  return 0;
}
