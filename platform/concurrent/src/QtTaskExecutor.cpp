#include "oso/concurrent/QtTaskExecutor.h"

#include "oso/base/ErrorCode.h"
#include "oso/logging/LogManager.h"

#include <thread>

namespace oso {

namespace {

/// QRunnable 包装器，执行 task 并在完成时通知 promise
class TaskRunnable : public QRunnable {
public:
    TaskRunnable(std::function<void()> fn, std::promise<void> promise)
        : m_fn(std::move(fn)), m_promise(std::move(promise)) {}

    void run() override {
        try {
            m_fn();
        } catch (...) {
            OSO_LOG_DEBUG("TaskRunnable存在未处理异常，建议在传入的回调函数中手动加异常处理逻辑");
        }
        m_promise.set_value();
    }

private:
    std::function<void()> m_fn;
    std::promise<void> m_promise;
};

}  // anonymous namespace

QtTaskExecutor::QtTaskExecutor(unsigned int maxThreadCount) {
    if (maxThreadCount > 0) {
        m_pool.setMaxThreadCount(static_cast<int>(maxThreadCount));
    }
}

QtTaskExecutor::~QtTaskExecutor() { waitAll(0); }

void QtTaskExecutor::submit(std::function<void()> task, int priority) {
    std::promise<void> promise;
    auto future = promise.get_future();

    auto runnable = std::make_unique<TaskRunnable>(std::move(task), std::move(promise));

    QMutexLocker locker(&m_futuresMutex);
    m_futures.erase(std::remove_if(m_futures.begin(), m_futures.end(),
                                   [](const std::future<void>& f) {
                                       return f.wait_for(std::chrono::seconds(0)) ==
                                              std::future_status::ready;
                                   }),
                    m_futures.end());
    m_futures.push_back(std::move(future));

    // 释放 runnable 给 QThreadPool（自动 delete 在 run() 之后）
    m_pool.start(runnable.release(), priority);
}

Result<void> QtTaskExecutor::waitAll(int64_t timeout) {
    std::vector<std::future<void>> snapshot;
    {
        QMutexLocker locker(&m_futuresMutex);
        m_futures.erase(std::remove_if(m_futures.begin(), m_futures.end(),
                                       [](const std::future<void>& f) {
                                           return f.wait_for(std::chrono::seconds(0)) ==
                                                  std::future_status::ready;
                                       }),
                        m_futures.end());
        snapshot = std::move(m_futures);
        m_futures.clear();
    }

    auto start = std::chrono::steady_clock::now();

    auto tmo = std::chrono::milliseconds(timeout);

    for (auto& f : snapshot) {
        if (tmo.count() > 0) {
            auto status = f.wait_for(std::chrono::milliseconds(0));
            while (status != std::future_status::ready) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);
                if (elapsed >= tmo) {
                    // 未完成的放回列表，调用方可重试
                    QMutexLocker locker(&m_futuresMutex);
                    for (auto& remaining : snapshot) {
                        if (remaining.wait_for(std::chrono::seconds(0)) !=
                            std::future_status::ready) {
                            m_futures.push_back(std::move(remaining));
                        }
                    }
                    return Result<void>::err(ErrorCode::ConcurrentTimeout, "waitAll timed out");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                status = f.wait_for(std::chrono::milliseconds(0));
            }
        } else {
            f.wait();
        }
    }
    return Result<void>::ok();
}

size_t QtTaskExecutor::pendingCount() const {
    QMutexLocker locker(&m_futuresMutex);
    size_t count = 0;
    for (const auto& f : m_futures) {
        if (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++count;
        }
    }
    return count;
}

}  // namespace oso
