// Транслятор MiniGroovy. Поки що реалізовано лексичний аналізатор (КП1).
//
// Використання:  minigroovy.exe [--trace] <file.mgy>
//                --trace — покроково показати роботу діаграми станів
// Коди виходу:   0 — аналіз успішний, 1 — лексична помилка,
//                2 — неправильні аргументи або файл не прочитано.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "lexer/lexer.hpp"
#include "report/report.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr std::string_view kSourceExtension = ".mgy";

void printUsage(std::ostream& out) {
    out << "MiniGroovy lexical analyzer\n"
        << "Usage: minigroovy [--trace] <file" << kSourceExtension << ">\n"
        << "Prints the symbol table, the identifier table and the constant table.\n"
        << "  --trace   show every step of the state diagram before the tables\n";
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // Текст програми — UTF-8; без цього кирилиця в повідомленнях про помилки
    // відображалася б у консолі Windows некоректно.
    SetConsoleOutputCP(CP_UTF8);
#endif

    bool trace = false;
    const char* fileArg = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(std::cout);
            return 0;
        }
        if (arg == "--trace") {
            trace = true;
        } else if (fileArg == nullptr && !arg.starts_with('-')) {
            fileArg = argv[i];
        } else {
            printUsage(std::cerr);
            return 2;
        }
    }
    if (fileArg == nullptr) {
        printUsage(std::cerr);
        return 2;
    }

    const std::filesystem::path path(fileArg);
    // binary — щоб ОС не перетворювала CR LF: кінці рядків обробляє сам автомат.
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: cannot open file '" << path.string() << "'\n";
        return 2;
    }
    if (path.extension() != kSourceExtension) {
        std::cerr << "Warning: MiniGroovy source files are expected to have the " << kSourceExtension
                  << " extension\n";
    }

    std::ostringstream text;
    text << file.rdbuf();

    minigroovy::Lexer lexer(text.str(), trace ? &std::cout : nullptr);
    const minigroovy::LexResult result = lexer.run();
    if (trace) std::cout << '\n';

    minigroovy::printTokenTable(std::cout, result);
    if (result.ok()) {
        std::cout << '\n';
        minigroovy::printIdentifierTable(std::cout, result);
        std::cout << '\n';
        minigroovy::printConstantTable(std::cout, result);
    }
    std::cout << '\n';
    minigroovy::printSummary(std::cout, result);
    return result.ok() ? 0 : 1;
}
