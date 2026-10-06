// Перевірки на прикладах програм з каталогу examples/.

#include <cctype>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "lexer/lexer.hpp"
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

}  // namespace

void runExampleTests(TestRunner& t, const std::filesystem::path& examplesDir) {
    const std::filesystem::path basePath = examplesDir / "base.mgy";
    t.check(std::filesystem::exists(basePath), "examples/base.mgy exists", basePath.string());
    if (!std::filesystem::exists(basePath)) return;

    // Базовий приклад повинен містити всі лексеми таблиці 2, крім пробільних
    // символів, кінців рядків і коментарів (рядки 45–50).
    const LexResult base = Lexer(readFile(basePath)).run();
    std::set<int> missing;
    for (int code = 1; code <= 52; ++code) {
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
