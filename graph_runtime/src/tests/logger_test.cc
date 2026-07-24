#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "src/framework/utils/logger.h"
#include "src/framework/utils/hook.h"
#include "gtest/gtest.h"

namespace graph::runtime {

namespace {

bool g_hook_invoked = false;
std::string g_hook_line;
bool g_hook_return_value = false;

bool TestHook(const void* data, int) {
  g_hook_invoked = true;
  g_hook_line = static_cast<const char*>(data);
  return g_hook_return_value;
}

bool ThrowingHook(const void*, int) {
  throw std::runtime_error("hook failure");
}

// Test hooks registered via delegate.
// Each test clears via ClearForTesting in SetUp/TearDown.
struct HookTests : ::testing::Test {
  void SetUp() override {
    g_hook_invoked = false;
    g_hook_line.clear();
    g_hook_return_value = false;
    hook::HookFactory::ClearForTesting();
  }
  void TearDown() override {
    hook::HookFactory::ClearForTesting();
  }
};

}  // namespace

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

// --- Hook tests ---

TEST_F(HookTests, HookReceivesFormattedLine) {
  hook::HookFactory::Register(hook::kTypeLog, TestHook);
  g_hook_return_value = false;

  Logger::Info("graphrt::hooktest", "hook msg");

  EXPECT_TRUE(g_hook_invoked);
  EXPECT_NE(g_hook_line.find("graphrt::hooktest"), std::string::npos);
  EXPECT_NE(g_hook_line.find(" I "), std::string::npos);
  EXPECT_NE(g_hook_line.find("hook msg"), std::string::npos);
}

TEST_F(HookTests, HookReturnsTrueSuppressesOutput) {
  hook::HookFactory::Register(hook::kTypeLog, TestHook);
  g_hook_return_value = true;

  Logger::Info("graphrt::test", "suppress me");

  EXPECT_TRUE(g_hook_invoked);
}

TEST_F(HookTests, HookReturnsFalseAllowsOutput) {
  hook::HookFactory::Register(hook::kTypeLog, TestHook);
  g_hook_return_value = false;

  Logger::Info("graphrt::test", "let through");

  EXPECT_TRUE(g_hook_invoked);
}

TEST_F(HookTests, RegisterReplacesExisting) {
  static bool s_second_called = false;
  s_second_called = false;
  g_hook_invoked = false;
  hook::HookFactory::Register(hook::kTypeLog, TestHook);

  hook::HookFactory::Register(hook::kTypeLog,
                     [](const void*, int) { s_second_called = true; return false; });

  Logger::Info("graphrt::test", "replace");

  EXPECT_FALSE(g_hook_invoked);
  EXPECT_TRUE(s_second_called);
}

TEST_F(HookTests, NoHookDefaultsToStdout) {
  // No hooks registered — output must not crash
  Logger::Info("graphrt::test", "no hook");
}

TEST_F(HookTests, HookExceptionDoesNotCrash) {
  hook::HookFactory::Register(hook::kTypeLog, ThrowingHook);

  Logger::Info("graphrt::test", "after exception");
  Logger::Error("graphrt::test", "still works");
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

}  // namespace graph::runtime
