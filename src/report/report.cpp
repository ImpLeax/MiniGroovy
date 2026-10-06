#include "report/report.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

#include "lexer/token.hpp"

namespace minigroovy {

namespace {

// Ширина стовпця «лексема»: за найдовшою лексемою, але в розумних межах.
constexpr std::size_t kMinLexemeWidth = 8;
constexpr std::size_t kMaxLexemeWidth = 32;

// Керівні символи у значеннях рядкових констант показуємо як \n, \t, \r.
std::string escapeForDisplay(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

template <typename Range, typename Projection>
std::size_t columnWidth(const Range& rows, Projection lexemeOf) {
    std::size_t width = kMinLexemeWidth;
    for (const auto& row : rows) width = std::max(width, lexemeOf(row).size());
    return std::min(width, kMaxLexemeWidth);
}

void printRule(std::ostream& out, std::size_t width) { out << std::string(width, '-') << '\n'; }

}  // namespace

void printTokenTable(std::ostream& out, const LexResult& result) {
    const std::size_t w = columnWidth(result.tokens, [](const Token& t) -> const std::string& { return t.lexeme; });
    const std::string header = std::format("{:>5}  {:>5}  {:<{}}  {:<10}  {:>4}  {:>5}", "#", "Line", "Lexeme", w,
                                           "Token", "Code", "Index");
    out << "Symbol table (tableOfSymb)\n" << header << '\n';
    printRule(out, header.size());

    int number = 0;
    for (const Token& t : result.tokens) {
        std::string row = std::format("{:>5}  {:>5}  {:<{}}  {:<10}  {:>4}", ++number, t.line, t.lexeme, w,
                                      tokenTypeName(t.type), t.code);
        if (t.index > 0) row += std::format("  {:>5}", t.index);
        out << row << '\n';
    }
}

void printIdentifierTable(std::ostream& out, const LexResult& result) {
    out << "Identifier table (tableOfId)\n" << std::format("{:>5}  {}\n", "Index", "Identifier");
    printRule(out, 24);
    for (std::size_t i = 0; i < result.identifiers.size(); ++i) {
        out << std::format("{:>5}  {}\n", i + 1, result.identifiers[i]);
    }
}

void printConstantTable(std::ostream& out, const LexResult& result) {
    const std::size_t w =
        columnWidth(result.constants, [](const ConstantEntry& c) -> const std::string& { return c.lexeme; });
    const std::string header = std::format("{:>5}  {:<{}}  {:<8}  {}", "Index", "Lexeme", w, "Token", "Value");
    out << "Constant table (tableOfConst)\n" << header << '\n';
    printRule(out, header.size());
    for (const ConstantEntry& c : result.constants) {
        out << std::format("{:>5}  {:<{}}  {:<8}  {}\n", c.index, c.lexeme, w, tokenTypeName(c.type),
                           escapeForDisplay(c.value));
    }
}

void printSummary(std::ostream& out, const LexResult& result) {
    if (result.ok()) {
        out << "Lexer: lexical analysis completed successfully\n";
        return;
    }
    const LexicalError& e = *result.error;
    out << std::format("Lexer: error at line {}, column {}: {}\n", e.line, e.column, e.message);
    out << std::format("Lexer: lexical analysis aborted in state {}\n", static_cast<int>(e.state));
}

}  // namespace minigroovy
