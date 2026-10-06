#include "lexer/token.hpp"

namespace minigroovy {

std::string_view tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Id: return "id";
        case TokenType::IntNum: return "intnum";
        case TokenType::FloatNum: return "floatnum";
        case TokenType::StrVal: return "strval";
        case TokenType::BoolVal: return "boolval";
        case TokenType::Keyword: return "keyword";
        case TokenType::Builtin: return "builtin";
        case TokenType::AssignOp: return "assign_op";
        case TokenType::AddOp: return "add_op";
        case TokenType::MultOp: return "mult_op";
        case TokenType::PowOp: return "pow_op";
        case TokenType::RelOp: return "rel_op";
        case TokenType::AndOp: return "and_op";
        case TokenType::OrOp: return "or_op";
        case TokenType::NotOp: return "not_op";
        case TokenType::Paren: return "paren";
        case TokenType::Brace: return "brace";
        case TokenType::Bracket: return "bracket";
        case TokenType::Punct: return "punct";
        case TokenType::RangeOp: return "range_op";
    }
    return "unknown";
}

}  // namespace minigroovy
