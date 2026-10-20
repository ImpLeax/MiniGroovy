// Перевірки на прикладах програм з каталогу examples/.

#include <cctype>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "lexer_tests.hpp"

namespace minigroovy::test {

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// Номер стану, у якому має зупинитися аналіз, — це число на початку імені
// файла в examples/errors/ (наприклад, 104_bad_escape.mgy -> 104).
int expectedStateOf(const std::filesystem::path& path) {
    const std::string name = path.stem().string();
    std::size_t digits = 0;
    while (digits < name.size() && std::isdigit(static_cast<unsigned char>(name[digits]))) ++digits;
    return digits == 0 ? -1 : std::stoi(name.substr(0, digits));
}

void checkValidProgram(TestRunner& t, const std::filesystem::path& path) {
    const LexResult result = Lexer(readFile(path)).run();
    std::string detail;
    if (result.error) detail = std::format("line {}: {}", result.error->line, result.error->message);
    t.check(result.ok(), "example " + path.filename().string() + " is lexically valid", detail);
}

// Номер рядка, у якому має зупинитися синтаксичний аналіз, записано
// в першому рядку файла коментарем «// Expected: line N».
int expectedLineOf(const std::string& source) {
    const std::string marker = "// Expected: line ";
    if (!source.starts_with(marker)) return -1;
    return std::stoi(source.substr(marker.size()));
}

// Програма лексично коректна, а синтаксичний аналіз має завершитися
// аварійно саме в рядку expectedLine (-1 — успішно).
void checkParse(TestRunner& t, const std::filesystem::path& path, int expectedLine) {
    const LexResult lexed = Lexer(readFile(path)).run();
    const ParseResult parsed = lexed.ok() ? Parser(lexed.tokens).run() : ParseResult{};
    std::string detail = "parsed successfully";
    if (!lexed.ok()) detail = "lexical error: " + lexed.error->message;
    if (parsed.error) detail = std::format("error at line {}: {}", parsed.error->line, parsed.error->message);

    const int actualLine = parsed.error ? parsed.error->line : -1;
    const bool ok = lexed.ok() && actualLine == expectedLine;
    const std::string name = expectedLine < 0 ? "example " + path.filename().string() + " is syntactically valid"
                                              : "syntax error example " + path.filename().string();
    t.check(ok, name, std::format("expected {}, got {}",
                                  expectedLine < 0 ? "success" : std::format("error at line {}", expectedLine),
                                  detail));
}

}  // namespace

void runExampleTests(TestRunner& t, const std::filesystem::path& examplesDir) {
    const std::filesystem::path basePath = examplesDir / "base.mgy";
    t.check(std::filesystem::exists(basePath), "examples/base.mgy exists", basePath.string());
    if (!std::filesystem::exists(basePath)) return;

    // Базовий приклад повинен містити всі лексеми таблиці 2, крім пробільних
    // символів, кінців рядків і коментарів (рядки 45–50).
    const LexResult base = Lexer(readFile(basePath)).run();
    std::set<int> missing;
    for (int code = 1; code <= 57; ++code) {
        if (code < 45 || code > 50) missing.insert(code);
    }
    for (const Token& token : base.tokens) missing.erase(token.code);
    std::string missingText;
    for (const int code : missing) missingText += std::to_string(code) + ' ';
    t.check(base.ok() && missing.empty(), "base example contains every lexeme of table 2",
            "missing codes: " + missingText);

    for (const auto& entry : std::filesystem::directory_iterator(examplesDir)) {
        if (entry.path().extension() == ".mgy") checkValidProgram(t, entry.path());
    }

    // Синтаксично коректні приклади. lexically_valid.mgy навмисно містить
    // синтаксичні помилки (перша — у рядку 3), tmp.mgy — чернетка, не перевіряється.
    for (const auto& entry : std::filesystem::directory_iterator(examplesDir)) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".mgy" || name == "tmp.mgy") continue;
        checkParse(t, entry.path(), name == "lexically_valid.mgy" ? 3 : -1);
    }

    // Кожен файл у examples/syntax_errors/ має зупинити розбір у рядку з коментаря «// Expected: line N».
    const std::filesystem::path syntaxErrorsDir = examplesDir / "syntax_errors";
    if (std::filesystem::exists(syntaxErrorsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(syntaxErrorsDir)) {
            if (entry.path().extension() != ".mgy") continue;
            checkParse(t, entry.path(), expectedLineOf(readFile(entry.path())));
        }
    }

    // Кожен файл у examples/errors/ має зупинити аналіз у стані з його імені.
    const std::filesystem::path errorsDir = examplesDir / "errors";
    if (!std::filesystem::exists(errorsDir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(errorsDir)) {
        if (entry.path().extension() != ".mgy") continue;
        const LexResult result = Lexer(readFile(entry.path())).run();
        const int expected = expectedStateOf(entry.path());
        const int actual = result.error ? static_cast<int>(result.error->state) : 0;
        t.check(actual == expected, "error example " + entry.path().filename().string(),
                std::format("expected state {}, got {}", expected, actual));
    }
}

}  // namespace minigroovy::test