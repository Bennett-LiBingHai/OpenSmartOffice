#include "oso/base/ErrorCode.h"
#include "oso/io/IFileSystem.h"
#include "oso/io/IStream.h"

#include <cstdio>
#include <filesystem>

namespace oso {

bool NativeFileSystem::exists(const std::string& path) const {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

Result<uint64_t> NativeFileSystem::fileSize(const std::string& path) const {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<uint64_t>::err(ErrorCode::IOFileNotFound, ec.message());
    }
    return Result<uint64_t>::ok(sz);
}

Result<std::unique_ptr<IStream>> NativeFileSystem::openRead(const std::string& path) {
    if (!exists(path)) {
        return Result<std::unique_ptr<IStream>>::err(ErrorCode::IOFileNotFound, path);
    }
    auto stream = std::make_unique<FileStream>(path, "rb");
    if (!stream->isOpen()) {
        return Result<std::unique_ptr<IStream>>::err(ErrorCode::IOReadError, path);
    }
    return Result<std::unique_ptr<IStream>>::ok(std::move(stream));
}

Result<std::unique_ptr<IStream>> NativeFileSystem::openWrite(const std::string& path) {
    auto stream = std::make_unique<FileStream>(path, "wb");
    if (!stream->isOpen()) {
        return Result<std::unique_ptr<IStream>>::err(ErrorCode::IOWriteError, path);
    }
    return Result<std::unique_ptr<IStream>>::ok(std::move(stream));
}

Result<void> NativeFileSystem::remove(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::remove(path, ec)) {
        if (ec) {
            return Result<void>::err(ErrorCode::IOAccessDenied, ec.message());
        }
    }
    return Result<void>::ok();
}

Result<std::vector<std::string>> NativeFileSystem::listDirectory(const std::string& path) const {
    std::vector<std::string> entries;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        entries.push_back(entry.path().filename().string());
    }
    if (ec) {
        return Result<std::vector<std::string>>::err(ErrorCode::IOFileNotFound, ec.message());
    }
    return Result<std::vector<std::string>>::ok(std::move(entries));
}

Result<void> NativeFileSystem::createDirectory(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec) && ec) {
        return Result<void>::err(ErrorCode::IOAccessDenied, ec.message());
    }
    return Result<void>::ok();
}

std::string NativeFileSystem::tempPath() const {
    return std::filesystem::temp_directory_path().string();
}

}  // namespace oso
