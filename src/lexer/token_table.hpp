#pragma once

#include <optional>
#include <string_view>

#include "lexer/token.hpp"

namespace minigroovy {

// Інформація про лексему, яка однозначно задається своїм написанням.
struct TokenInfo {
    TokenType type;
    int code;  // код рядка таблиці 2
};

// Таблиця лексем мови: ключові слова, вбудовані функції, логічні літерали,
// оператори та розділювачі. Ідентифікатори й числові/рядкові константи сюди
// не входять — їхній токен визначає заключний стан автомата.
// Повертає std::nullopt, якщо лексеми в таблиці немає.
std::optional<TokenInfo> lookupLexeme(std::string_view lexeme);

}  // namespace minigroovy
