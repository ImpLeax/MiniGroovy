#pragma once

#include <cstddef>
#include <ostream>
#include <string>

#include "lexer/char_class.hpp"
#include "lexer/state_table.hpp"

namespace minigroovy {

// Трасування роботи автомата (режим --trace): один рядок на кожен прочитаний
// символ і окремий рядок про дії в заключному стані. Дає змогу покроково
// простежити, як діаграма станів розпізнає лексеми.

// Опис одного кроку автомата.
struct TraceStep {
    int number;            // порядковий номер кроку
    std::size_t position;  // індекс прочитаного байта у тексті
    int line;
    int column;
    std::string charText;  // прочитаний символ у зручному для читання вигляді
    CharClass cls;
    State from;
    Transition transition;
    std::string lexeme;    // поточна лексема після кроку
};

void printTraceHeader(std::ostream& out);
void printTraceStep(std::ostream& out, const TraceStep& step);
void printTraceAction(std::ostream& out, const std::string& text);

}  // namespace minigroovy
