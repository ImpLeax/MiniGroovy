#pragma once

#include <string>
#include <string_view>

namespace minigroovy {

// Категорія лексеми — стовпець «Токен» таблиці 2 специфікації.
enum class TokenType {
    Id,
    IntNum,
    FloatNum,
    StrVal,
    BoolVal,
    Keyword,
    Builtin,
    AssignOp,
    AddOp,
    MultOp,
    PowOp,
    RelOp,
    AndOp,
    OrOp,
    NotOp,
    Paren,
    Brace,
    Bracket,
    Punct,
    RangeOp,
};

// Назва токена так, як вона записана в таблиці 2 (id, intnum, rel_op, ...).
std::string_view tokenTypeName(TokenType type);

// Коди рядків таблиці 2 для лексем, що визначаються не написанням, а станом автомата.
namespace token_code {
inline constexpr int kId = 1;
inline constexpr int kIntNum = 2;
inline constexpr int kFloatNum = 3;
inline constexpr int kStrVal = 4;
}  // namespace token_code

// Один запис таблиці розбору (таблиці символів програми).
struct Token {
    int line = 0;        // номер рядка вхідної програми (з 1)
    int column = 0;      // номер стовпця початку лексеми (з 1, у символах)
    std::string lexeme;  // лексема так, як вона записана в тексті (рядок — разом із лапками)
    TokenType type = TokenType::Id;
    int code = 0;        // код рядка таблиці 2
    int index = 0;       // індекс у таблиці ідентифікаторів або констант; 0 — для інших лексем
};

}  // namespace minigroovy
