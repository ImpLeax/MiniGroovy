#pragma once

#include "lexer/char_class.hpp"

namespace minigroovy {

// Стани діаграми станів. Номери збігаються з діаграмою JFLAP
// (КП1/diagram/minigroovy_lexer.jff) та символьним представленням (symbolic.md).
enum class State : int {
    Start = 0,            // початок лексеми; пробіли пропускаються петлею

    Ident = 1,            // читання ідентифікатора
    IdentEnd = 2,         // * id / keyword / builtin / boolval

    IntDigits = 3,        // цифри цілої частини
    IntDot = 4,           // цифри + крапка: чекаємо цифру дробової частини
    IntEnd = 5,           // * intnum
    FracDigits = 6,       // цифри дробової частини
    FloatEnd = 7,         // * floatnum
    IntBeforeDot = 8,     // ** intnum, крапка після якого числу не належить ("12.", "0..100")

    DotRead = 9,          // прочитано '.'
    StarRead = 10,        // прочитано '*'
    CompareRead = 11,     // прочитано '<', '>', '=' або '!'
    AmpRead = 12,         // прочитано '&'
    BarRead = 13,         // прочитано '|'
    SlashRead = 14,       // прочитано '/'
    Comment = 15,         // тіло коментаря
    CommentEnd = 16,      // * кінець коментаря
    String = 17,          // тіло рядкової константи
    StringEscape = 18,    // прочитано '\' у рядку
    StringEnd = 19,       // strval

    TwoCharOp = 20,       // ** <= >= == != && || ..
    OneCharOpBack = 21,   // * односимвольний оператор: * < > = ! / .
    SingleChar = 22,      // + - ( ) { } [ ] ; ,

    CrRead = 23,          // прочитано CR
    EndOfLine = 24,       // кінець рядка LF або CRLF
    EndOfLineCr = 25,     // * кінець рядка CR
    EndOfFile = 26,       // кінець файла

    ErrIllegalChar = 101,     // символ поза алфавітом (або '\' поза рядком)
    ErrExpectedAmp = 102,     // одиночний '&'
    ErrExpectedBar = 103,     // одиночний '|'
    ErrBadEscape = 104,       // невідома escape-послідовність
    ErrUnclosedString = 105,  // рядок не закрито до кінця рядка/файла
    ErrTabInString = 106,     // табуляція всередині рядка
    ErrBadNumber = 107,       // до числа прилипли літери: 2value, 1e5, 3.5abc
};

// Функція переходів δ(стан, символ). Перехід шукається в такому порядку:
// спершу за конкретним символом, потім за класом символу, потім за класом other.
State nextState(State state, int ch, CharClass cls);

// Чи є стан заключним (множина F).
bool isFinal(State state);

// Чи є стан станом помилки (множина F_error).
bool isError(State state);

// Скільки символів треба повернути у вхідний потік у заключному стані:
// 1 для F_star (*), 2 для F_dstar (**), 0 для решти.
int charsToPutBack(State state);

}  // namespace minigroovy
