#include "lexer/token_table.hpp"

#include <unordered_map>

namespace minigroovy {

std::optional<TokenInfo> lookupLexeme(std::string_view lexeme) {
    using T = TokenType;
    // Коди збігаються з номерами рядків таблиці 2 специфікації.
    static const std::unordered_map<std::string_view, TokenInfo> table = {
        {"true", {T::BoolVal, 5}},      {"false", {T::BoolVal, 6}},
        {"int", {T::Keyword, 7}},       {"float", {T::Keyword, 8}},
        {"boolean", {T::Keyword, 9}},   {"String", {T::Keyword, 10}},
        {"void", {T::Keyword, 11}},     {"final", {T::Keyword, 12}},
        {"enum", {T::Keyword, 13}},     {"if", {T::Keyword, 14}},
        {"else", {T::Keyword, 15}},     {"while", {T::Keyword, 16}},
        {"for", {T::Keyword, 17}},      {"return", {T::Keyword, 18}},
        {"read", {T::Builtin, 19}},     {"print", {T::Builtin, 20}},
        {"=", {T::AssignOp, 21}},       {"+", {T::AddOp, 22}},
        {"-", {T::AddOp, 23}},          {"*", {T::MultOp, 24}},
        {"/", {T::MultOp, 25}},         {"**", {T::PowOp, 26}},
        {"<", {T::RelOp, 27}},          {"<=", {T::RelOp, 28}},
        {">", {T::RelOp, 29}},          {">=", {T::RelOp, 30}},
        {"==", {T::RelOp, 31}},         {"!=", {T::RelOp, 32}},
        {"&&", {T::AndOp, 33}},         {"||", {T::OrOp, 34}},
        {"!", {T::NotOp, 35}},          {"(", {T::Paren, 36}},
        {")", {T::Paren, 37}},          {"{", {T::Brace, 38}},
        {"}", {T::Brace, 39}},          {"[", {T::Bracket, 40}},
        {"]", {T::Bracket, 41}},        {";", {T::Punct, 42}},
        {",", {T::Punct, 43}},          {".", {T::Punct, 44}},
        // Рядки 45–50 (пробіли, кінці рядків, коментарі) у таблицю розбору не потрапляють.
        {"type", {T::Keyword, 51}},     {"..", {T::RangeOp, 52}},
    };

    const auto it = table.find(lexeme);
    if (it == table.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace minigroovy
