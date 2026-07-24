#include <thread>
#include <vector>

#include "src/public/logger.h"
#include "gtest/gtest.h"

namespace graph::runtime {

TEST(LoggerTest, LogLevelToString_AllLevels) {
  EXPECT_STREQ(LogLevelToString(LogLevel::kFatal), "FATAL");
  EXPECT_STREQ(LogLevelToString(LogLevel::kError), "ERROR");
  EXPECT_STREQ(LogLevelToString(LogLevel::kWarn), "WARN");
  EXPECT_STREQ(LogLevelToString(LogLevel::kInfo), "INFO");
  EXPECT_STREQ(LogLevelToString(LogLevel::kDebug), "DEBUG");
}

TEST(LoggerTest, LogLevelToString_OutOfRange) {
  EXPECT_STREQ(LogLevelToString(static_cast<LogLevel>(99)), "UNKNOWN");
}

TEST(LoggerTest, InfoDoesNotCrash) {
  Logger::Info("graphrt::test", "hello");
}

TEST(LoggerTest, AllLevelsDoNotCrash) {
  Logger::Debug("t", "d");
  Logger::Info("t", "i");
  Logger::Warn("t", "w");
  Logger::Error("t", "e");
  Logger::Fatal("t", "f");
}

TEST(LoggerTest, ConcurrentLogging) {
  static constexpr int kThreads = 8;
  static constexpr int kLogsPerThread = 50;
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([i] {
      for (int j = 0; j < kLogsPerThread; ++j) {
        Logger::Info("graphrt::test", "thread");
        Logger::Debug("graphrt::test", "debug");
        Logger::Error("graphrt::test", "err");
      }
    });
  }
  for (auto& t : threads) t.join();
}

TEST(LoggerTest, RepeatedCalls) {
  for (int i = 0; i < 1000; ++i) {
    Logger::Info("graphrt::test", "repeat");
  }
}

}  // namespace graph::runtime
