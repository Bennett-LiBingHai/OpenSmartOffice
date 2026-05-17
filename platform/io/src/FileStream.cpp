#include "oso/base/ErrorCode.h"
#include "oso/io/IStream.h"

#include <algorithm>

namespace oso {

FileStream::FileStream(const std::string& path, const char* mode)
    : m_file(std::fopen(path.c_str(), mode)) {}

FileStream::~FileStream() {
    if (m_file != nullptr) {
        std::fclose(m_file);
    }
}

FileStream::FileStream(FileStream&& other) noexcept : m_file(other.m_file) {
    other.m_file = nullptr;
}

FileStream& FileStream::operator=(FileStream&& other) noexcept {
    if (this != &other) {
        if (m_file != nullptr) {
            std::fclose(m_file);
        }
        m_file = other.m_file;
        other.m_file = nullptr;
    }
    return *this;
}

Result<size_t> FileStream::read(uint8_t* buffer, size_t maxLen) {
    if (m_file == nullptr) {
        return Result<size_t>::err(ErrorCode::IOReadError, "File not open");
    }
    size_t n = std::fread(buffer, 1, maxLen, m_file);
    if (std::ferror(m_file) != 0) {
        return Result<size_t>::err(ErrorCode::IOReadError, "fread failed");
    }
    return Result<size_t>::ok(n);
}

Result<std::vector<uint8_t>> FileStream::readAll() {
    if (m_file == nullptr) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::IOReadError, "File not open");
    }
    auto cur = std::ftell(m_file);
    std::fseek(m_file, 0, SEEK_END);
    auto end = std::ftell(m_file);
    std::fseek(m_file, cur, SEEK_SET);

    std::vector<uint8_t> data(static_cast<size_t>(end - cur));
    size_t n = std::fread(data.data(), 1, data.size(), m_file);
    if (std::ferror(m_file) != 0) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::IOReadError, "fread failed");
    }
    data.resize(n);
    return Result<std::vector<uint8_t>>::ok(std::move(data));
}

Result<void> FileStream::write(const uint8_t* data, size_t len) {
    if (m_file == nullptr) {
        return Result<void>::err(ErrorCode::IOWriteError, "File not open");
    }
    size_t written = std::fwrite(data, 1, len, m_file);
    if (written != len) {
        return Result<void>::err(ErrorCode::IOWriteError, "fwrite failed");
    }
    return Result<void>::ok();
}

Result<uint64_t> FileStream::seek(int64_t offset, StreamSeek whence) {
    if (m_file == nullptr) {
        return Result<uint64_t>::err(ErrorCode::IOReadError, "File not open");
    }
    if (std::fseek(m_file, offset, static_cast<int>(whence)) != 0) {
        return Result<uint64_t>::err(ErrorCode::IOReadError, "fseek failed");
    }
    return Result<uint64_t>::ok(static_cast<uint64_t>(std::ftell(m_file)));
}

Result<uint64_t> FileStream::tell() const {
    if (m_file == nullptr) {
        return Result<uint64_t>::err(ErrorCode::IOReadError, "File not open");
    }
    return Result<uint64_t>::ok(static_cast<uint64_t>(std::ftell(m_file)));
}

bool FileStream::isOpen() const { return m_file != nullptr; }

Result<void> FileStream::flush() {
    if (m_file == nullptr) {
        return Result<void>::err(ErrorCode::IOWriteError, "File not open");
    }
    std::fflush(m_file);
    return Result<void>::ok();
}

Result<void> FileStream::close() {
    if (m_file == nullptr) {
        return Result<void>::err(ErrorCode::IOWriteError, "File not open");
    }
    if (std::fclose(m_file) != 0) {
        return Result<void>::err(ErrorCode::IOWriteError, "fclose failed");
    }
    m_file = nullptr;
    return Result<void>::ok();
}

Result<uint64_t> FileStream::size() const {
    if (m_file == nullptr) {
        return Result<uint64_t>::err(ErrorCode::IOReadError, "File not open");
    }
    auto cur = std::ftell(m_file);
    std::fseek(m_file, 0, SEEK_END);
    auto end = std::ftell(m_file);
    std::fseek(m_file, cur, SEEK_SET);  // restore position
    return Result<uint64_t>::ok(static_cast<uint64_t>(end));
}

}  // namespace oso
