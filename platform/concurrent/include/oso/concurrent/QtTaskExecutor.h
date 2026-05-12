#pragma once

#include "oso/concurrent/ITaskExecutor.h"

#include <QMutex>
#include <QThreadPool>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace oso {

/// ITaskExecutor 的 QThreadPool 实现。
///
/// 内部使用 QThreadPool::start(QRunnable*, priority) 支持任务优先级，
/// std::promise/future 追踪完成状态。
/// 对外只暴露 ITaskExecutor 的纯 C++ 接口。
class QtTaskExecutor : public ITaskExecutor {
   public:
    explicit QtTaskExecutor(int maxThreadCount = std::thread::hardware_concurrency());
    ~QtTaskExecutor() override;

    QtTaskExecutor(const QtTaskExecutor&) = delete;
    QtTaskExecutor& operator=(const QtTaskExecutor&) = delete;

    void submit(std::function<void()> task, int priority = 0) override;
    Result<void> waitAll(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) override;
    size_t pendingCount() const override;

   private:
    QThreadPool m_pool;
    mutable QMutex m_futuresMutex;
    std::vector<std::future<void>> m_futures;
};

}  // namespace oso
