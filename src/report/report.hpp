#pragma once

#include <ostream>

#include "lexer/lexer.hpp"

namespace minigroovy {

// Таблиця розбору: номер запису, рядок, лексема, токен, код табл. 2, індекс.
void printTokenTable(std::ostream& out, const LexResult& result);

// Таблиця ідентифікаторів: індекс та ідентифікатор.
void printIdentifierTable(std::ostream& out, const LexResult& result);

// Таблиця констант: індекс, лексема, токен, значення.
void printConstantTable(std::ostream& out, const LexResult& result);

// Підсумкове повідомлення про успішність або неуспішність аналізу.
void printSummary(std::ostream& out, const LexResult& result);

}  // namespace minigroovy
