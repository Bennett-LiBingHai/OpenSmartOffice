#include "oso/io/IStream.h"
#include "oso/base/ErrorCode.h"
#include <cstring>

namespace oso {

Result<size_t> MemoryStream::read(uint8_t* buffer, size_t maxLen) {
    if (!m_open) return Result<size_t>::err(ErrorCode::IO_ReadError, "Stream closed");
    size_t available = m_data.size() - m_pos;
    size_t toRead = std::min(maxLen, available);
    if (toRead > 0) {
        std::memcpy(buffer, m_data.data() + m_pos, toRead);
        m_pos += toRead;
    }
    return Result<size_t>::ok(toRead);
}

Result<std::vector<uint8_t>> MemoryStream::readAll() {
    if (!m_open) return Result<std::vector<uint8_t>>::err(ErrorCode::IO_ReadError, "Stream closed");
    std::vector<uint8_t> result(m_data.begin() + m_pos, m_data.end());
    m_pos = m_data.size();
    return Result<std::vector<uint8_t>>::ok(std::move(result));
}

Result<void> MemoryStream::write(const uint8_t* data, size_t len) {
    if (!m_open) return Result<void>::err(ErrorCode::IO_WriteError, "Stream closed");
    if (m_pos + len > m_data.size()) {
        m_data.resize(m_pos + len);
    }
    std::memcpy(m_data.data() + m_pos, data, len);
    m_pos += len;
    return Result<void>::ok();
}

Result<uint64_t> MemoryStream::seek(int64_t offset, StreamSeek whence) {
    int64_t newPos = m_pos;
    if (whence == StreamSeek::START) { // SEEK_SET
        newPos = offset;
    } else if (whence == StreamSeek::CUR) { // SEEK_CUR
        newPos = m_pos + offset;
    } else if (whence == StreamSeek::END) { // SEEK_END
        newPos = static_cast<int64_t>(m_data.size()) + offset;
    }
    if (newPos < 0 || static_cast<size_t>(newPos) > m_data.size()) {
        return Result<uint64_t>::err(ErrorCode::IO_ReadError, "Seek out of bounds");
    }
    m_pos = static_cast<size_t>(newPos);
    return Result<uint64_t>::ok(m_pos);
}

Result<uint64_t> MemoryStream::tell() const {
    return Result<uint64_t>::ok(m_pos);
}

Result<void> MemoryStream::close() {
    m_open = false;
    return Result<void>::ok();
}

} // namespace oso
