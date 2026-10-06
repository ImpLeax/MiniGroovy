#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace minigroovy {

// Службове значення «кінець файла». Його повертає nextChar(), коли текст закінчився.
inline constexpr int kEof = -1;

// Класи символів діаграми станів (див. КП1/diagram/symbolic.md).
// Переходи визначаються не для окремих символів, а для класів — так діаграма
// лишається компактною.
enum class CharClass {
    Letter,     // a-z, A-Z та '_'
    Digit,      // 0-9
    Dot,        // '.'
    Quote,      // '"'
    Backslash,  // '\'
    Ws,         // пробіл і горизонтальна табуляція
    Nl,         // LF
    Cr,         // CR
    Eof,        // кінець файла
    Special,    // + - * / = < > ! & | ( ) { } [ ] ; , — переходи за ними задаються посимвольно
    Illegal,    // символ поза алфавітом MiniGroovy (у т.ч. будь-який байт UTF-8 > 127)
};

// Визначає клас символу. ch — байт тексту (0..255) або kEof.
CharClass classOfChar(int ch);

// Назва класу для діагностики й тестів.
std::string_view charClassName(CharClass cls);

// Довжина (у байтах) символу UTF-8, що починається з text[pos].
// Зіпсована послідовність вважається одним байтом.
std::size_t codePointLength(std::string_view text, std::size_t pos);

// Кількість символів UTF-8 у text[from, to) — для обчислення номера стовпця.
int countCodePoints(std::string_view text, std::size_t from, std::size_t to);

// Символ для повідомлення про помилку: керівні символи показуються як \n, \t, \xNN.
std::string printableChar(std::string_view text, std::size_t pos);

}  // namespace minigroovy
