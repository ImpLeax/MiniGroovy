// Точка входу тестів.
// Використання: minigroovy_tests.exe [каталог examples]

#include <filesystem>
#include <iostream>

#include "lexer_tests.hpp"

int main(int argc, char** argv) {
    minigroovy::test::TestRunner runner;
    minigroovy::test::runLexerTests(runner);
    if (argc > 1) {
        minigroovy::test::runExampleTests(runner, std::filesystem::path(argv[1]));
    }

    std::cout << "Tests passed: " << (runner.total() - runner.failed()) << "/" << runner.total() << '\n';
    return runner.failed() == 0 ? 0 : 1;
}
