#pragma once

#include "oso/base/ErrorCode.h"
#include <string>
#include <utility>

namespace oso {

// Result<T>：要么存储成功值，要么存储错误信息
// 不使用异常，访问 value() 前必须先调用 isOk() / isErr() 检查状态

template <typename T>
class Result {
public:
    // 构造成功结果
    static Result ok(T value) {
        return Result(ErrorCode::Ok, std::move(value), "");
    }

    // 构造错误结果
    static Result err(ErrorCode code, std::string message = "") {
        return Result(code, T{}, std::move(message));
    }

    // 是否成功
    bool isOk() const { return m_code.isOk(); }
    // 是否失败
    bool isErr() const { return m_code.isErr(); }

    // 获取值（非常量）
    T& value() { return m_value; }
    // 获取值（常量）
    const T& value() const { return m_value; }

    // 转移值所有权（移动语义）
    T&& takeValue() { return std::move(m_value); }

    // 获取错误码
    const ErrorCode& error() const { return m_code; }
    // 获取错误信息
    const std::string& message() const { return m_message; }

private:
    // 私有构造函数，通过静态方法 ok/err 创建对象
    Result(ErrorCode code, T value, std::string message)
        : m_code(code), m_value(std::move(value)), m_message(std::move(message)) {}

    ErrorCode m_code;      // 错误码
    T m_value;             // 成功时存储的值
    std::string m_message; // 错误信息
};

// void 类型特化（无返回值的结果）
template <>
class Result<void> {
public:
    static Result ok() { return Result(ErrorCode::Ok, ""); }
    static Result err(ErrorCode code, std::string message = "") {
        return Result(code, std::move(message));
    }

    bool isOk() const { return m_code.isOk(); }
    bool isErr() const { return m_code.isErr(); }

    const ErrorCode& error() const { return m_code; }
    const std::string& message() const { return m_message; }

private:
    Result(ErrorCode code, std::string message)
        : m_code(code), m_message(std::move(message)) {}

    ErrorCode m_code;      // 错误码
    std::string m_message; // 错误信息
};

// 快速失败宏：执行表达式，失败则直接返回错误，省去if (res.isErr()) return err;（_expr返回Result类型）
#define OSO_TRY(expr)                                          \
    do {                                                       \
        auto _oso_result = (expr);                             \
        if (_oso_result.isErr()) {                             \
            return Result<std::remove_reference_t<             \
                decltype(_oso_result.value())>>::err(          \
                _oso_result.error(), _oso_result.message());   \
        }                                                      \
    } while (false)

// 赋值+快速失败宏：执行表达式，成功则赋值给变量，失败则返回错误（_expr返回Result类型）
#define OSO_TRY_ASSIGN(_var, _expr)                            \
    auto _oso_result = (_expr);                                \
    if (_oso_result.isErr()) {                                 \
        return Result<std::remove_reference_t<                 \
            decltype(_oso_result.value())>>::err(              \
            _oso_result.error(), _oso_result.message());       \
    }                                                          \
    _var = _oso_result.takeValue();

} // namespace oso