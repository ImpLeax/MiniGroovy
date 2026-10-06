#include "lexer/state_table.hpp"

#include <map>
#include <utility>

namespace minigroovy {

namespace {

using S = State;
using C = CharClass;

// Перехід за конкретним символом: δ(from, 'ch') = to.
struct CharTransition {
    S from;
    char ch;
    S to;
};

// Перехід за класом символу: δ(from, cls) = to.
struct ClassTransition {
    S from;
    C cls;
    S to;
};

// Перехід за класом other: δ(from, other) = to.
struct OtherTransition {
    S from;
    S to;
};

// ---------------------------------------------------------------------------
// Функція переходів δ. Записана по гілках діаграми, у тому ж порядку,
// що й у symbolic.md. Змінюючи таблицю, оновіть і діаграму
// (КП1/diagram/generate_diagrams.py), щоб вони не розійшлися.
// ---------------------------------------------------------------------------

constexpr CharTransition kCharTransitions[] = {
    // Оператори з можливим другим символом: ** <= >= == !=
    {S::Start, '*', S::StarRead},
    {S::StarRead, '*', S::TwoCharOp},
    {S::Start, '<', S::CompareRead},
    {S::Start, '>', S::CompareRead},
    {S::Start, '=', S::CompareRead},
    {S::Start, '!', S::CompareRead},
    {S::CompareRead, '=', S::TwoCharOp},
    // && та || — одиночні & і | операторами не є
    {S::Start, '&', S::AmpRead},
    {S::AmpRead, '&', S::TwoCharOp},
    {S::Start, '|', S::BarRead},
    {S::BarRead, '|', S::TwoCharOp},
    // Ділення або початок коментаря //
    {S::Start, '/', S::SlashRead},
    {S::SlashRead, '/', S::Comment},
    // Рядок: табуляцію треба записувати як \t
    {S::String, '\t', S::ErrTabInString},
    // Escape-послідовності \n \t \r (\" і \\ — через класи quote і bslash)
    {S::StringEscape, 'n', S::String},
    {S::StringEscape, 't', S::String},
    {S::StringEscape, 'r', S::String},
    // Односимвольні лексеми
    {S::Start, '+', S::SingleChar},
    {S::Start, '-', S::SingleChar},
    {S::Start, '(', S::SingleChar},
    {S::Start, ')', S::SingleChar},
    {S::Start, '{', S::SingleChar},
    {S::Start, '}', S::SingleChar},
    {S::Start, '[', S::SingleChar},
    {S::Start, ']', S::SingleChar},
    {S::Start, ';', S::SingleChar},
    {S::Start, ',', S::SingleChar},
};

constexpr ClassTransition kClassTransitions[] = {
    // Ідентифікатор
    {S::Start, C::Letter, S::Ident},
    {S::Ident, C::Letter, S::Ident},
    {S::Ident, C::Digit, S::Ident},
    // Числа
    {S::Start, C::Digit, S::IntDigits},
    {S::IntDigits, C::Digit, S::IntDigits},
    {S::IntDigits, C::Dot, S::IntDot},
    {S::IntDigits, C::Letter, S::ErrBadNumber},
    {S::IntDot, C::Digit, S::FracDigits},
    {S::FracDigits, C::Digit, S::FracDigits},
    {S::FracDigits, C::Letter, S::ErrBadNumber},
    // Крапка та оператор діапазону ..
    {S::Start, C::Dot, S::DotRead},
    {S::DotRead, C::Dot, S::TwoCharOp},
    // Коментар триває до кінця рядка або файла
    {S::Comment, C::Nl, S::CommentEnd},
    {S::Comment, C::Cr, S::CommentEnd},
    {S::Comment, C::Eof, S::CommentEnd},
    {S::Comment, C::Illegal, S::ErrIllegalChar},
    // Рядкова константа
    {S::Start, C::Quote, S::String},
    {S::String, C::Quote, S::StringEnd},
    {S::String, C::Backslash, S::StringEscape},
    {S::String, C::Nl, S::ErrUnclosedString},
    {S::String, C::Cr, S::ErrUnclosedString},
    {S::String, C::Eof, S::ErrUnclosedString},
    {S::String, C::Illegal, S::ErrIllegalChar},
    {S::StringEscape, C::Quote, S::String},
    {S::StringEscape, C::Backslash, S::String},
    // Пробіли, кінці рядків, кінець файла
    {S::Start, C::Ws, S::Start},
    {S::Start, C::Nl, S::EndOfLine},
    {S::Start, C::Cr, S::CrRead},
    {S::CrRead, C::Nl, S::EndOfLine},
    {S::Start, C::Eof, S::EndOfFile},
};

constexpr OtherTransition kOtherTransitions[] = {
    {S::Start, S::ErrIllegalChar},
    {S::Ident, S::IdentEnd},
    {S::IntDigits, S::IntEnd},
    {S::IntDot, S::IntBeforeDot},
    {S::FracDigits, S::FloatEnd},
    {S::DotRead, S::OneCharOpBack},
    {S::StarRead, S::OneCharOpBack},
    {S::CompareRead, S::OneCharOpBack},
    {S::AmpRead, S::ErrExpectedAmp},
    {S::BarRead, S::ErrExpectedBar},
    {S::SlashRead, S::OneCharOpBack},
    {S::Comment, S::Comment},
    {S::String, S::String},
    {S::StringEscape, S::ErrBadEscape},
    {S::CrRead, S::EndOfLineCr},
};

// Таблиці пошуку, побудовані один раз із масивів вище.
struct Lookup {
    std::map<std::pair<S, char>, S> byChar;
    std::map<std::pair<S, C>, S> byClass;
    std::map<S, S> byOther;

    Lookup() {
        for (const auto& t : kCharTransitions) byChar.emplace(std::pair{t.from, t.ch}, t.to);
        for (const auto& t : kClassTransitions) byClass.emplace(std::pair{t.from, t.cls}, t.to);
        for (const auto& t : kOtherTransitions) byOther.emplace(t.from, t.to);
    }
};

const Lookup& lookup() {
    static const Lookup instance;
    return instance;
}

}  // namespace

Transition findTransition(State state, int ch, CharClass cls) {
    const Lookup& table = lookup();

    // 1) за конкретним символом
    if (ch != kEof) {
        const auto it = table.byChar.find({state, static_cast<char>(ch)});
        if (it != table.byChar.end()) return {it->second, TransitionVia::Char};
    }
    // 2) за класом символу
    if (const auto it = table.byClass.find({state, cls}); it != table.byClass.end()) {
        return {it->second, TransitionVia::Class};
    }
    // 3) за класом other — він є для кожного незаключного стану
    return {table.byOther.at(state), TransitionVia::Other};
}

State nextState(State state, int ch, CharClass cls) { return findTransition(state, ch, cls).to; }

bool isFinal(State state) {
    switch (state) {
        case S::IdentEnd:
        case S::IntEnd:
        case S::FloatEnd:
        case S::IntBeforeDot:
        case S::CommentEnd:
        case S::StringEnd:
        case S::TwoCharOp:
        case S::OneCharOpBack:
        case S::SingleChar:
        case S::EndOfLine:
        case S::EndOfLineCr:
        case S::EndOfFile:
            return true;
        default:
            return isError(state);
    }
}

bool isError(State state) { return static_cast<int>(state) >= 101; }

int charsToPutBack(State state) {
    switch (state) {
        // F_star: символ, що завершив лексему, належить наступній лексемі
        case S::IdentEnd:
        case S::IntEnd:
        case S::FloatEnd:
        case S::CommentEnd:
        case S::OneCharOpBack:
        case S::EndOfLineCr:
            return 1;
        // F_dstar: крапка й символ після неї не належать числу
        case S::IntBeforeDot:
            return 2;
        default:
            return 0;
    }
}

}  // namespace minigroovy
