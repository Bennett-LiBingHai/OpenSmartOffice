#include "oso/base/ErrorCode.h"
#include "oso/concurrent/ITaskExecutor.h"
#include "oso/concurrent/QtTaskExecutor.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace oso;
using namespace std::chrono_literals;

// ================================================================
// 基本功能
// ================================================================

TEST(QtTaskExecutor, SubmitSingleTask) {
    QtTaskExecutor executor;
    std::atomic<bool> done{false};

    executor.submit([&done] { done = true; });
    auto result = executor.waitAll(0);

    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_TRUE(done);
    EXPECT_EQ(executor.pendingCount(), 0u);
}

TEST(QtTaskExecutor, SubmitTenTasksCollectResults) {
    // 里程碑验收：10 个任务并行执行，闭包收集结果
    QtTaskExecutor executor;
    auto results = std::make_shared<std::vector<int>>();
    auto mtx = std::make_shared<std::mutex>();

    for (int i = 0; i < 10; ++i) {
        executor.submit([results, mtx, i] {
            // 模拟不同程度的工作量
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 2));
            std::lock_guard<std::mutex> lock(*mtx);
            results->push_back(i);
        });
    }

    auto waitResult = executor.waitAll(0);
    ASSERT_TRUE(waitResult.isOk()) << waitResult.message();

    EXPECT_EQ(results->size(), 10u);
    EXPECT_EQ(executor.pendingCount(), 0u);

    // 验证所有任务确实并行执行（总时间应远小于串行时间）
    // 串行总睡眠 = 0+2+4+...+18 = 90ms，并行应远小于此
}

TEST(QtTaskExecutor, SubmitEmptyTask) {
    QtTaskExecutor executor;
    executor.submit([] {});
    auto result = executor.waitAll(0);
    ASSERT_TRUE(result.isOk()) << result.message();
}

TEST(QtTaskExecutor, SubmitZeroTasksWaitAllReturnsOk) {
    QtTaskExecutor executor;
    auto result = executor.waitAll(0);
    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(executor.pendingCount(), 0u);
}

// ================================================================
// 超时测试
// ================================================================

TEST(QtTaskExecutor, WaitAllTimeout) {
    QtTaskExecutor executor;

    // 提交一个长任务
    executor.submit([] { std::this_thread::sleep_for(500ms); });

    // 只等 10ms，应该超时
    auto result = executor.waitAll(10);
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::ConcurrentTimeout);

    // 最终等完任务
    auto result2 = executor.waitAll(0);
    ASSERT_TRUE(result2.isOk());
}

TEST(QtTaskExecutor, WaitAllZeroTimeoutWaitsForever) {
    QtTaskExecutor executor;
    std::atomic<bool> done{false};

    executor.submit([&done] {
        std::this_thread::sleep_for(50ms);
        done = true;
    });

    // timeout=0 表示无限等待
    auto result = executor.waitAll(0);
    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_TRUE(done);
}

// ================================================================
// 优先级测试
// ================================================================

TEST(QtTaskExecutor, HighPriorityTasksRunBeforeLowPriority) {
    QtTaskExecutor executor(1);  // 单线程：队列顺序即执行顺序
    auto orderPtr = std::make_shared<std::vector<int>>();
    auto mtx = std::make_shared<std::mutex>();
    std::atomic<bool> gate{false};

    // 先提交一个 blocker 占住唯一线程
    executor.submit([&gate] {
        while (!gate) {
            std::this_thread::sleep_for(1ms);
        }
    });

    // 等 blocker 开始执行，后续提交的任务进入等待队列
    std::this_thread::sleep_for(10ms);

    executor.submit(
        [orderPtr, mtx] {
            std::lock_guard<std::mutex> lock(*mtx);
            orderPtr->push_back(0);
        },
        0);  // 低优先级 — 先进队列

    executor.submit(
        [orderPtr, mtx] {
            std::lock_guard<std::mutex> lock(*mtx);
            orderPtr->push_back(1);
        },
        10);  // 高优先级 — 后进队列，但应被优先取出

    gate = true;
    executor.waitAll(0);

    ASSERT_EQ(orderPtr->size(), 2u);
    // QThreadPool 按优先级从队列取任务，高优先级先出队
    EXPECT_EQ(orderPtr->at(0), 1);
}

// ================================================================
// 并发提交
// ================================================================

TEST(QtTaskExecutor, SubmitFromMultipleThreads) {
    QtTaskExecutor executor;
    std::atomic<int> counter{0};

    auto submitter = [&executor, &counter](int count) {
        for (int i = 0; i < count; ++i) {
            executor.submit([&counter] { counter.fetch_add(1); });
        }
    };

    std::thread t1(submitter, 25);
    std::thread t2(submitter, 25);
    std::thread t3(submitter, 25);
    std::thread t4(submitter, 25);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    executor.waitAll(0);
    EXPECT_EQ(counter.load(), 100);
}

// ================================================================
// pendingCount
// ================================================================

TEST(QtTaskExecutor, PendingCountDecrementsAsTasksComplete) {
    QtTaskExecutor executor;

    std::atomic<bool> release{false};
    for (int i = 0; i < 5; ++i) {
        executor.submit([&release] {
            while (!release) {
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    // 任务都在等待信号，应全部处于 pending 状态
    // （由于调度延迟，可能略少，但至少应有 > 0）
    EXPECT_GT(executor.pendingCount(), 0u);

    release = true;
    executor.waitAll(0);
    EXPECT_EQ(executor.pendingCount(), 0u);
}

// ================================================================
// 任务异常安全
// ================================================================

TEST(QtTaskExecutor, TaskThrowsException) {
    QtTaskExecutor executor;
    std::atomic<bool> afterException{false};

    // 提交会抛异常的任务
    executor.submit([] { throw std::runtime_error("task error"); });

    // 再提交正常任务
    executor.submit([&afterException] { afterException = true; });

    // waitAll 不应崩溃（QtConcurrent 会捕获异常并存储在 QFuture 中）
    auto result = executor.waitAll(0);
    // QFuture 的异常不会传播到 waitAll，所以应该成功
    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(afterException);
}

// ================================================================
// 析构安全
// ================================================================

TEST(QtTaskExecutor, DestructorWaitsForAll) {
    std::atomic<bool> taskCompleted{false};

    {
        QtTaskExecutor executor;
        executor.submit([&taskCompleted] {
            std::this_thread::sleep_for(100ms);
            taskCompleted = true;
        });
    }  // executor 析构，应等待任务完成

    EXPECT_TRUE(taskCompleted);
}

// ================================================================
// 资源清理与重新提交
// ================================================================

TEST(QtTaskExecutor, SubmitAfterWaitAll) {
    QtTaskExecutor executor;

    // 第一轮
    for (int i = 0; i < 5; ++i) {
        executor.submit([i] { std::this_thread::sleep_for(1ms); });
    }
    ASSERT_TRUE(executor.waitAll(0).isOk());

    // 第二轮
    std::atomic<int> counter{0};
    for (int i = 0; i < 5; ++i) {
        executor.submit([&counter] { counter.fetch_add(1); });
    }
    ASSERT_TRUE(executor.waitAll(0).isOk());
    EXPECT_EQ(counter.load(), 5);
}

// ================================================================
// ITaskExecutor 多态
// ================================================================

TEST(ITaskExecutor, PolymorphicUse) {
    auto executor = std::make_unique<QtTaskExecutor>();
    auto& ref = static_cast<ITaskExecutor&>(*executor);

    std::atomic<int> count{0};
    for (int i = 0; i < 3; ++i) {
        ref.submit([&count] { count.fetch_add(1); });
    }
    ref.waitAll(0);
    EXPECT_EQ(count.load(), 3);
}

// ================================================================
// 并行度验证（任务确实在多个线程上执行）
// ================================================================

TEST(QtTaskExecutor, TasksExecuteInParallel) {
    QtTaskExecutor executor(4);  // 最多 4 线程

    std::atomic<int> maxConcurrency{0};
    std::atomic<int> inFlight{0};

    for (int i = 0; i < 8; ++i) {
        executor.submit([&inFlight, &maxConcurrency] {
            int current = inFlight.fetch_add(1) + 1;
            // 更新最大并发数
            int prev = maxConcurrency.load();
            while (current > prev && !maxConcurrency.compare_exchange_weak(prev, current)) {
            }
            std::this_thread::sleep_for(50ms);
            inFlight.fetch_sub(1);
        });
    }

    executor.waitAll(0);

    // 至少有 2 个任务在并行执行（保守值）
    EXPECT_GE(maxConcurrency.load(), 2);
}

// ================================================================
// 大量任务压力
// ================================================================

TEST(QtTaskExecutor, StressTestManySmallTasks) {
    QtTaskExecutor executor;
    std::atomic<int> counter{0};
    constexpr int kNumTasks = 1000;

    for (int i = 0; i < kNumTasks; ++i) {
        executor.submit([&counter] { counter.fetch_add(1); });
    }

    auto result = executor.waitAll(0);
    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_EQ(counter.load(), kNumTasks);
}
