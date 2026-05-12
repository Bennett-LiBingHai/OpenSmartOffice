#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/base/ErrorCode.h"

#include <zip.h>
#include <cstdlib>
#include <cstring>

namespace oso {

LibzipZipArchive::LibzipZipArchive() : m_archive(nullptr) {}

LibzipZipArchive::~LibzipZipArchive() {
    if (m_archive) {
        zip_close(m_archive);
    }
}

LibzipZipArchive::LibzipZipArchive(LibzipZipArchive&& other) noexcept
    : m_archive(other.m_archive)
{
    other.m_archive = nullptr;
}

LibzipZipArchive& LibzipZipArchive::operator=(LibzipZipArchive&& other) noexcept {
    if (this != &other) {
        if (m_archive) {
            zip_close(m_archive);
        }
        m_archive = other.m_archive;
        other.m_archive = nullptr;
    }
    return *this;
}

Result<void> LibzipZipArchive::open(const std::string& path) {
    if (m_archive) {
        return Result<void>::err(ErrorCode::OOXML_ZipCorrupted, "Archive already open");
    }

    int error = 0;
    m_archive = zip_open(path.c_str(), ZIP_RDONLY, &error);

    if (!m_archive) {
        switch (error) {
            case ZIP_ER_NOENT:
                return Result<void>::err(ErrorCode::IO_FileNotFound, "File not found: " + path);
            case ZIP_ER_NOZIP:
                return Result<void>::err(ErrorCode::OOXML_ZipCorrupted, "Not a valid zip file: " + path);
            case ZIP_ER_OPEN:
                return Result<void>::err(ErrorCode::IO_AccessDenied, "Cannot open file: " + path);
            case ZIP_ER_READ:
                return Result<void>::err(ErrorCode::IO_ReadError, "Read error: " + path);
            default:
                return Result<void>::err(ErrorCode::OOXML_ZipCorrupted,
                    "Failed to open zip, error code: " + std::to_string(error));
        }
    }

    return Result<void>::ok();
}

Result<void> LibzipZipArchive::create(const std::string& path) {
    if (m_archive) {
        zip_close(m_archive);
        m_archive = nullptr;
    }

    int error = 0;
    m_archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);

    if (!m_archive) {
        switch (error) {
            case ZIP_ER_OPEN:
                return Result<void>::err(ErrorCode::IO_AccessDenied, "Cannot create file: " + path);
            case ZIP_ER_WRITE:
                return Result<void>::err(ErrorCode::IO_WriteError, "Write error creating: " + path);
            default:
                return Result<void>::err(ErrorCode::OOXML_ZipWriteError,
                    "Failed to create zip, error code: " + std::to_string(error));
        }
    }

    return Result<void>::ok();
}

Result<void> LibzipZipArchive::writeEntry(const std::string& name,
                                          const std::vector<uint8_t>& data) {
    if (!m_archive) {
        return Result<void>::err(ErrorCode::OOXML_ZipWriteError, "Archive not created");
    }

    // 拷贝数据到堆上，libzip 用 freep=1 接管所有权，在 source 释放时调用 free()
    auto* buf = static_cast<uint8_t*>(std::malloc(data.size()));
    if (!buf) {
        return Result<void>::err(ErrorCode::OOXML_ZipWriteError, "Memory allocation failed for: " + name);
    }
    std::memcpy(buf, data.data(), data.size());

    auto* src = zip_source_buffer(m_archive, buf, data.size(), 1);
    if (!src) {
        std::free(buf);
        return Result<void>::err(ErrorCode::OOXML_ZipWriteError,
            "Failed to create source for: " + name + " - " +
            std::string(zip_strerror(m_archive)));
    }

    zip_int64_t idx = zip_file_add(m_archive, name.c_str(), src, ZIP_FL_OVERWRITE);
    if (idx < 0) {
        zip_source_free(src);
        return Result<void>::err(ErrorCode::OOXML_ZipWriteError,
            "Failed to add entry: " + name + " - " +
            std::string(zip_strerror(m_archive)));
    }

    return Result<void>::ok();
}

Result<std::vector<IZipArchive::Entry>> LibzipZipArchive::entries() const {
    if (!m_archive) {
        return Result<std::vector<Entry>>::err(ErrorCode::OOXML_ZipCorrupted, "Archive not open");
    }

    std::vector<Entry> result;
    zip_int64_t count = zip_get_num_entries(m_archive, 0);
    if (count < 0) {
        return Result<std::vector<Entry>>::err(ErrorCode::OOXML_ZipCorrupted,
            "Failed to get entry count: " + std::string(zip_strerror(m_archive)));
    }

    for (zip_int64_t i = 0; i < count; ++i) {
        zip_stat_t stat;
        if (zip_stat_index(m_archive, static_cast<zip_uint64_t>(i), 0, &stat) != 0) {
            return Result<std::vector<Entry>>::err(ErrorCode::OOXML_ZipCorrupted,
                "Failed to stat entry: " + std::string(zip_strerror(m_archive)));
        }

        Entry entry;
        entry.name = stat.name;
        entry.compressedSize = stat.comp_size;
        entry.uncompressedSize = stat.size;
        entry.isDirectory = (stat.name[0] != '\0' && stat.name[strlen(stat.name) - 1] == '/');
        result.push_back(std::move(entry));
    }

    return Result<std::vector<Entry>>::ok(std::move(result));
}

Result<std::vector<uint8_t>> LibzipZipArchive::readEntry(const std::string& name) {
    if (!m_archive) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::OOXML_ZipCorrupted, "Archive not open");
    }

    zip_int64_t idx = zip_name_locate(m_archive, name.c_str(), 0);
    if (idx < 0) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::OOXML_ZipEntryMissing,
            "Entry not found: " + name);
    }

    zip_stat_t stat;
    if (zip_stat_index(m_archive, static_cast<zip_uint64_t>(idx), 0, &stat) != 0) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::OOXML_ZipCorrupted,
            "Failed to stat entry: " + std::string(zip_strerror(m_archive)));
    }

    zip_file_t* file = zip_fopen_index(m_archive, static_cast<zip_uint64_t>(idx), 0);
    if (!file) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::OOXML_ZipCorrupted,
            "Failed to open entry: " + std::string(zip_strerror(m_archive)));
    }

    std::vector<uint8_t> buffer(stat.size);
    zip_int64_t bytesRead = zip_fread(file, buffer.data(), stat.size);
    zip_fclose(file);

    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != stat.size) {
        return Result<std::vector<uint8_t>>::err(ErrorCode::IO_ReadError,
            "Failed to read entry data: " + name);
    }

    return Result<std::vector<uint8_t>>::ok(std::move(buffer));
}

Result<void> LibzipZipArchive::close() {
    if (!m_archive) {
        return Result<void>::ok();
    }

    if (zip_close(m_archive) != 0) {
        std::string err = zip_strerror(m_archive);
        m_archive = nullptr;
        return Result<void>::err(ErrorCode::OOXML_ZipCorrupted, "Failed to close archive: " + err);
    }

    m_archive = nullptr;
    return Result<void>::ok();
}

bool LibzipZipArchive::isOpen() const {
    return m_archive != nullptr;
}

} // namespace oso
