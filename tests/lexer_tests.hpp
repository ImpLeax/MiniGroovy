#pragma once

#include <filesystem>

#include "test_framework.hpp"

namespace minigroovy::test {

// Модульні тести лексера на коротких фрагментах коду.
void runLexerTests(TestRunner& t);

// Перевірки на файлах з каталогу examples/ (базовий приклад тощо).
void runExampleTests(TestRunner& t, const std::filesystem::path& examplesDir);

}  // namespace minigroovy::test
