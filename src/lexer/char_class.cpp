#include "lexer/char_class.hpp"

namespace minigroovy {

namespace {

// Спеціальні знаки, переходи за якими в діаграмі задаються посимвольно.
constexpr std::string_view kSpecialChars = "+-*/=<>!&|(){}[];,";

bool isContinuationByte(unsigned char byte) { return (byte & 0xC0) == 0x80; }

}  // namespace

CharClass classOfChar(int ch) {
    if (ch == kEof) return CharClass::Eof;
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') return CharClass::Letter;
    if (ch >= '0' && ch <= '9') return CharClass::Digit;
    switch (ch) {
        case '.': return CharClass::Dot;
        case '"': return CharClass::Quote;
        case '\\': return CharClass::Backslash;
        case ' ':
        case '\t': return CharClass::Ws;
        case '\n': return CharClass::Nl;
        case '\r': return CharClass::Cr;
        default: break;
    }
    if (kSpecialChars.find(static_cast<char>(ch)) != std::string_view::npos) return CharClass::Special;
    return CharClass::Illegal;
}

std::string_view charClassName(CharClass cls) {
    switch (cls) {
        case CharClass::Letter: return "Letter";
        case CharClass::Digit: return "Digit";
        case CharClass::Dot: return "dot";
        case CharClass::Quote: return "quote";
        case CharClass::Backslash: return "bslash";
        case CharClass::Ws: return "ws";
        case CharClass::Nl: return "nl";
        case CharClass::Cr: return "cr";
        case CharClass::Eof: return "eof";
        case CharClass::Special: return "special";
        case CharClass::Illegal: return "illegal";
    }
    return "?";
}

std::size_t codePointLength(std::string_view text, std::size_t pos) {
    // Довжина символу визначається першим байтом:
    // 0xxxxxxx -> 1, 110xxxxx -> 2, 1110xxxx -> 3, 11110xxx -> 4;
    // наступні байти мають вигляд 10xxxxxx.
    const auto first = static_cast<unsigned char>(text[pos]);
    std::size_t length = 1;
    if (first >= 0xC0 && first <= 0xDF) length = 2;
    else if (first >= 0xE0 && first <= 0xEF) length = 3;
    else if (first >= 0xF0 && first <= 0xF7) length = 4;

    if (length == 1 || pos + length > text.size()) return 1;
    for (std::size_t i = 1; i < length; ++i) {
        if (!isContinuationByte(static_cast<unsigned char>(text[pos + i]))) return 1;
    }
    return length;
}

int countCodePoints(std::string_view text, std::size_t from, std::size_t to) {
    int count = 0;
    for (std::size_t pos = from; pos < to && pos < text.size(); pos += codePointLength(text, pos)) {
        ++count;
    }
    return count;
}

std::string printableChar(std::string_view text, std::size_t pos) {
    if (pos >= text.size()) return "end of file";

    const auto byte = static_cast<unsigned char>(text[pos]);
    switch (byte) {
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        default: break;
    }
    if (byte < 32 || byte == 127) {
        constexpr std::string_view hex = "0123456789ABCDEF";
        return std::string("\\x") + hex[byte >> 4] + hex[byte & 15];
    }
    return std::string(text.substr(pos, codePointLength(text, pos)));
}

}  // namespace minigroovy
