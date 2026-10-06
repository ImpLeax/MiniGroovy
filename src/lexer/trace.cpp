#include "lexer/trace.hpp"

#include <format>

namespace minigroovy {

namespace {

// Запис переходу так само, як у символьному представленні діаграми: δ(1, Letter) = 1.
std::string transitionText(const TraceStep& step) {
    std::string input;
    switch (step.transition.via) {
        case TransitionVia::Char: input = step.charText; break;
        case TransitionVia::Class: input = std::string(charClassName(step.cls)); break;
        case TransitionVia::Other: input = "other"; break;
    }
    return std::format("δ({}, {}) = {}", static_cast<int>(step.from), input, static_cast<int>(step.transition.to));
}

}  // namespace

void printTraceHeader(std::ostream& out) {
    out << std::format("{:>5}  {:>5}  {:>8}  {:<6}  {:<8}  {:<24}  {}\n", "Step", "Pos", "Line:Col", "Char", "Class",
                       "Transition", "Lexeme");
    out << std::string(80, '-') << '\n';
}

void printTraceStep(std::ostream& out, const TraceStep& step) {
    // δ займає 2 байти в UTF-8, тому вирівнюємо стовпець переходу вручну.
    const std::string transition = transitionText(step);
    const std::size_t visibleWidth = transition.size() - 1;
    const std::string padding(visibleWidth < 24 ? 24 - visibleWidth : 0, ' ');

    out << std::format("{:>5}  {:>5}  {:>8}  {:<6}  {:<8}  ", step.number, step.position,
                       std::format("{}:{}", step.line, step.column), step.charText, charClassName(step.cls))
        << transition << padding << "  " << step.lexeme << '\n';
}

void printTraceAction(std::ostream& out, const std::string& text) { out << "             => " << text << '\n'; }

}  // namespace minigroovy
