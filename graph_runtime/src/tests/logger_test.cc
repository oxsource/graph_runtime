#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "src/public/logger.h"
#include "src/public/graph_runtime.h"
#include "gtest/gtest.h"

namespace graph::runtime {

// --- LogLevelToString tests ---

TEST(LoggerTest, LogLevelToString_AllLevels) {
  EXPECT_STREQ(LogLevelToString(LogLevel::kFatal), "F");
  EXPECT_STREQ(LogLevelToString(LogLevel::kError), "E");
  EXPECT_STREQ(LogLevelToString(LogLevel::kWarn), "W");
  EXPECT_STREQ(LogLevelToString(LogLevel::kInfo), "I");
  EXPECT_STREQ(LogLevelToString(LogLevel::kDebug), "D");
}

TEST(LoggerTest, LogLevelToString_OutOfRange) {
  EXPECT_STREQ(LogLevelToString(static_cast<LogLevel>(99)), "U");
}

// --- Default output tests ---

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

// --- Hook table tests ---

namespace {

bool g_hook_invoked = false;
std::string g_hook_line;
bool g_hook_return_value = false;

bool TestHook(const void* data, int flag) {
  g_hook_invoked = true;
  g_hook_line = static_cast<const char*>(data);
  return g_hook_return_value;
}

const GraphHookEntity kSingleHook[] = {
  { kHookTypeLogIntercept, TestHook },
  { kHookTypeSentinel, nullptr },
};

const GraphHookEntity kMultiHook[] = {
  { kHookTypeLogIntercept, TestHook },
  { kHookTypeLogIntercept, TestHook },
  { kHookTypeSentinel, nullptr },
};

}  // namespace

TEST(LoggerTest, HookReceivesFormattedLine) {
  g_hook_invoked = false;
  g_hook_line.clear();
  g_hook_return_value = false;

  GraphRuntime runtime;
  runtime.SetGlobalHook(kSingleHook);

  Logger::Info("graphrt::hooktest", "hook msg");

  EXPECT_TRUE(g_hook_invoked);
  EXPECT_NE(g_hook_line.find("[graphrt::hooktest]"), std::string::npos);
  EXPECT_NE(g_hook_line.find("[I]"), std::string::npos);
  EXPECT_NE(g_hook_line.find("hook msg"), std::string::npos);

  runtime.SetGlobalHook(nullptr);
}

TEST(LoggerTest, HookReturnsTrueSuppressesOutput) {
  g_hook_invoked = false;
  g_hook_line.clear();
  g_hook_return_value = true;

  GraphRuntime runtime;
  runtime.SetGlobalHook(kSingleHook);

  Logger::Info("graphrt::test", "suppress me");

  EXPECT_TRUE(g_hook_invoked);

  runtime.SetGlobalHook(nullptr);
}

TEST(LoggerTest, HookReturnsFalseAllowsOutput) {
  g_hook_invoked = false;
  g_hook_line.clear();
  g_hook_return_value = false;

  GraphRuntime runtime;
  runtime.SetGlobalHook(kSingleHook);

  Logger::Info("graphrt::test", "let through");

  EXPECT_TRUE(g_hook_invoked);

  runtime.SetGlobalHook(nullptr);
}

TEST(LoggerTest, MultipleHooksBothCalled) {
  g_hook_invoked = false;
  g_hook_line.clear();
  g_hook_return_value = false;

  GraphRuntime runtime;
  runtime.SetGlobalHook(kMultiHook);

  Logger::Info("graphrt::test", "multi");

  EXPECT_TRUE(g_hook_invoked);

  runtime.SetGlobalHook(nullptr);
}

TEST(LoggerTest, ClearHookRestoresDefault) {
  g_hook_invoked = false;
  g_hook_return_value = false;

  GraphRuntime runtime;
  runtime.SetGlobalHook(kSingleHook);
  runtime.SetGlobalHook(nullptr);

  Logger::Info("graphrt::test", "no hook");

  EXPECT_FALSE(g_hook_invoked);
}

// --- Exception safety ---

bool ThrowingHook(const void*, int) {
  throw std::runtime_error("hook failure");
}

TEST(LoggerTest, HookExceptionDoesNotCrash) {
  const GraphHookEntity kThrowingHook[] = {
    { kHookTypeLogIntercept, ThrowingHook },
    { kHookTypeSentinel, nullptr },
  };

  GraphRuntime runtime;
  runtime.SetGlobalHook(kThrowingHook);

  // Should not crash despite the hook throwing
  Logger::Info("graphrt::test", "after exception");
  Logger::Error("graphrt::test", "still works");

  runtime.SetGlobalHook(nullptr);
}

// --- Concurrent logging ---

TEST(LoggerTest, ConcurrentLogging) {
  static constexpr int kThreads = 8;
  static constexpr int kLogsPerThread = 50;
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([] {
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

TEST(LoggerTest, ConcurrentHookSwapNoCrash) {
  static constexpr int kThreads = 8;
  static constexpr int kLogsPerThread = 40;

  GraphRuntime runtime;
  std::atomic<bool> running{true};

  // Thread that continuously swaps the hook table
  std::thread swapper([&] {
    while (running.load()) {
      runtime.SetGlobalHook(kSingleHook);
      std::this_thread::yield();
      runtime.SetGlobalHook(nullptr);
      std::this_thread::yield();
    }
  });

  // Logging threads
  std::vector<std::thread> loggers;
  for (int i = 0; i < kThreads; ++i) {
    loggers.emplace_back([&] {
      for (int j = 0; j < kLogsPerThread; ++j) {
        Logger::Info("graphrt::test", "concurrent swap");
        Logger::Debug("graphrt::test", "debug");
        Logger::Error("graphrt::test", "err");
      }
    });
  }

  for (auto& t : loggers) t.join();
  running.store(false);
  swapper.join();

  runtime.SetGlobalHook(nullptr);
}

}  // namespace graph::runtime
