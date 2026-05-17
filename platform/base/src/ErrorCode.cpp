#include "oso/base/ErrorCode.h"

namespace oso {

/// 将错误码转换为可读的字符串名称，未知值返回 "Unknown(N)"
std::string ErrorCode::toString() const {
    switch (m_value) {
        case Ok:
            return "Ok";

        case IOFileNotFound:
            return "IOFileNotFound";
        case IOAccessDenied:
            return "IOAccessDenied";
        case IOReadError:
            return "IOReadError";
        case IOWriteError:
            return "IOWriteError";
        case IOUnexpectedEof:
            return "IOUnexpectedEof";

        case ConcurrentQueueFull:
            return "ConcurrentQueueFull";
        case ConcurrentShutdown:
            return "ConcurrentShutdown";
        case ConcurrentTimeout:
            return "ConcurrentTimeout";

        case OOXMLZipCorrupted:
            return "OOXMLZipCorrupted";
        case OOXMLZipEntryMissing:
            return "OOXMLZipEntryMissing";
        case OOXMLXmlParseError:
            return "OOXMLXmlParseError";
        case OOXMLInvalidSchema:
            return "OOXMLInvalidSchema";
        case OOXMLMissingPart:
            return "OOXMLMissingPart";
        case OOXMLBadContentType:
            return "OOXMLBadContentType";
        case OOXMLZipWriteError:
            return "OOXMLZipWriteError";

        case FormulaSyntaxError:
            return "FormulaSyntaxError";
        case FormulaCircularRef:
            return "FormulaCircularRef";
        case FormulaUnknownFunc:
            return "FormulaUnknownFunc";
        case FormulaTypeError:
            return "FormulaTypeError";

        case RenderFontNotFound:
            return "RenderFontNotFound";
        case RenderImageDecode:
            return "RenderImageDecode";

        case DOMInvalidNodeType:
            return "DOMInvalidNodeType";
        case DOMInvalidOperation:
            return "DOMInvalidOperation";
        case DOMIndexOutOfRange:
            return "DOMIndexOutOfRange";
        case DOMDocumentCorrupted:
            return "DOMDocumentCorrupted";

        default:
            return "Unknown(" + std::to_string(raw()) + ")";
    }
}

}  // namespace oso
