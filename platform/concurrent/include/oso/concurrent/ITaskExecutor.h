#pragma once

#include "oso/base/Result.h"

#include <chrono>
#include <functional>

namespace oso {

/// 任务执行器抽象接口。
///
/// 提供 submit/waitAll 统一调度 API，隐藏底层线程模型。
/// 阶段一使用 QtTaskExecutor（QThreadPool），阶段二可替换为
/// std::thread + 手写优先级队列实现，无需修改调用方。
class ITaskExecutor {
   public:
    virtual ~ITaskExecutor() = default;

    /// 提交一个任务到后台线程池异步执行。
    /// @param task   要执行的可调用对象
    /// @param priority  优先级（越大越高），实现可用作线程优先级提示
    virtual void submit(std::function<void()> task, int priority = 0) = 0;

    /// 等待所有已提交任务完成。
    /// @param timeout  超时时间，0 表示无限等待
    /// @return  成功返回 Ok，超时返回 Concurrent_Timeout
    virtual Result<void> waitAll(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) = 0;

    /// 返回当前未完成的任务数。
    virtual size_t pendingCount() const = 0;
};

}  // namespace oso
