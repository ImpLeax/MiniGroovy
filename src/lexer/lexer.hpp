#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "lexer/state_table.hpp"
#include "lexer/token.hpp"

namespace minigroovy {

// Запис таблиці констант.
struct ConstantEntry {
    std::string lexeme;  // як у тексті програми (рядок — з лапками та escape-послідовностями)
    TokenType type;      // intnum, floatnum або strval
    std::string value;   // значення: для strval — без лапок і з розкодованими escape-ами
    int index;           // індекс у таблиці констант (з 1)
};

// Лексична помилка, на якій аналіз аварійно завершився.
struct LexicalError {
    int line;
    int column;
    State state;          // заключний стан, у якому виявлено помилку
    std::string message;  // діагностичне повідомлення англійською
};

// Результат лексичного аналізу.
struct LexResult {
    std::vector<Token> tokens;               // таблиця розбору (таблиця символів програми)
    std::vector<std::string> identifiers;    // таблиця ідентифікаторів: identifiers[i - 1] має індекс i
    std::vector<ConstantEntry> constants;    // таблиця констант
    std::optional<LexicalError> error;       // заповнено, якщо аналіз завершився аварійно

    bool ok() const { return !error.has_value(); }
};

// Лексичний аналізатор MiniGroovy — програмний імітатор діаграми станів.
// Зупиняється на першій лексичній помилці (аварійне завершення).
class Lexer {
public:
    explicit Lexer(std::string source);

    // Розбирає весь текст програми.
    LexResult run();

private:
    // Читання символу та повернення символів у вхідний потік («зірочка»).
    int nextChar();
    void putCharBack(int count);

    // Семантичні процедури заключного стану. ch — символ, яким автомат
    // прийшов у цей стан. Повертає false, коли аналіз треба завершити.
    bool processing(State state, int ch);

    bool finishInteger(State state);
    bool finishFloat(State state);
    void finishString();
    void addFixedToken();
    void reportError(State state, int ch);

    void addToken(TokenType type, int code, int index);
    int identifierIndex(const std::string& name);
    int constantIndex(TokenType type, const std::string& value);
    void fail(State state, std::size_t errorPos, std::string message);
    int columnAt(std::size_t pos) const;

    std::string source_;
    std::size_t pos_ = 0;           // індекс наступного байта, який прочитає nextChar()
    int line_ = 1;                  // поточний номер рядка
    std::size_t lineStart_ = 0;     // індекс першого байта поточного рядка

    std::string lexeme_;            // символи поточної лексеми
    std::size_t lexemeStart_ = 0;   // індекс першого байта поточної лексеми

    LexResult result_;
    std::unordered_map<std::string, int> identifierIndex_;
    std::unordered_map<std::string, int> constantIndex_;
};

}  // namespace minigroovy
