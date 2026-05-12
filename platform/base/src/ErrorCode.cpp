#include "oso/base/ErrorCode.h"

namespace oso {

/// 将错误码转换为可读的字符串名称，未知值返回 "Unknown(N)"
std::string ErrorCode::toString() const {
    switch (m_value) {
        case Ok: return "Ok";

        case IO_FileNotFound:      return "IO_FileNotFound";
        case IO_AccessDenied:      return "IO_AccessDenied";
        case IO_ReadError:         return "IO_ReadError";
        case IO_WriteError:        return "IO_WriteError";
        case IO_UnexpectedEof:     return "IO_UnexpectedEof";

        case Concurrent_QueueFull: return "Concurrent_QueueFull";
        case Concurrent_Shutdown:  return "Concurrent_Shutdown";
        case Concurrent_Timeout:   return "Concurrent_Timeout";

        case OOXML_ZipCorrupted:    return "OOXML_ZipCorrupted";
        case OOXML_ZipEntryMissing: return "OOXML_ZipEntryMissing";
        case OOXML_XmlParseError:   return "OOXML_XmlParseError";
        case OOXML_InvalidSchema:   return "OOXML_InvalidSchema";
        case OOXML_MissingPart:     return "OOXML_MissingPart";
        case OOXML_BadContentType:  return "OOXML_BadContentType";
        case OOXML_ZipWriteError:   return "OOXML_ZipWriteError";

        case Formula_SyntaxError:   return "Formula_SyntaxError";
        case Formula_CircularRef:   return "Formula_CircularRef";
        case Formula_UnknownFunc:   return "Formula_UnknownFunc";
        case Formula_TypeError:     return "Formula_TypeError";

        case Render_FontNotFound:   return "Render_FontNotFound";
        case Render_ImageDecode:    return "Render_ImageDecode";

        case DOM_InvalidNodeType:   return "DOM_InvalidNodeType";
        case DOM_InvalidOperation:  return "DOM_InvalidOperation";
        case DOM_IndexOutOfRange:   return "DOM_IndexOutOfRange";
        case DOM_DocumentCorrupted: return "DOM_DocumentCorrupted";

        default: return "Unknown(" + std::to_string(raw()) + ")";
    }
}

} // namespace oso
