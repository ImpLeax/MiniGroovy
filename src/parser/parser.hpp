#pragma once

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "lexer/token.hpp"

namespace minigroovy {

// Синтаксична помилка, на якій розбір аварійно завершився.
struct SyntaxError {
    int line;
    int column;
    std::string message;  // діагностичне повідомлення англійською
};

// Результат синтаксичного аналізу.
struct ParseResult {
    std::optional<SyntaxError> error;  // заповнено, якщо розбір завершився аварійно

    bool ok() const { return !error.has_value(); }
};

// Синтаксичний аналізатор MiniGroovy методом рекурсивного спуску.
// Кожному нетерміналу граматики (розд. 7 специфікації) відповідає метод parseName().
// Зупиняється на першій синтаксичній помилці (аварійне завершення).
class Parser {
public:
    // tokens — таблиця розбору від лексера; trace — потік для журналу розбору (nullptr — без журналу).
    explicit Parser(const std::vector<Token>& tokens, std::ostream* trace = nullptr);

    // Розбирає всю програму.
    ParseResult run();

private:
    friend class TraceScope;

    // 7.1 Синтаксичні правила високого рівня
    void parseProgram();
    void parseGlobalItem();
    void parseDeclaration();
    void parseLocalDeclaration();
    void parseVarDeclaration();
    void parseConstDeclaration();
    void parseArrayDeclaration();
    void parseEnumDeclaration();
    void parseTypeDeclaration();
    void parseArraySize();
    void parseInitList();
    void parseEnumItems();
    void parseRangeBound();
    void parseType();
    void parseFunctionDecl();
    void parseReturnType();
    void parseParamList();
    void parseParameter();

    // 7.2 Інструкції та керуючі конструкції
    void parseStatementList();
    void parseBlockItem();
    void parseStatement();
    void parseBlock();
    void parseAssignment();
    void parseTarget();
    void parseOutput();
    void parseIf();
    void parseSwitch();
    void parseSwitchSection();
    void parseSwitchLabel();
    void parseCaseValue();
    void parseWhile();
    void parseFor();
    void parseForInit();
    void parseForVarDecl();
    void parseForUpdate();
    void parseCallStatement();
    void parseReturn();
    void parseBreak();
    void parseFunctionCall();
    void parseArguments();

    // 7.3 Вирази
    void parseExpression();
    void parseOrExpr();
    void parseAndExpr();
    void parseEqualityExpr();
    void parseRelExpr();
    void parseAddExpr();
    void parseTerm();
    void parseUnaryExpr();
    void parsePowerExpr();
    void parsePrimary();
    void parseNameExpr();
    void parseReadCall();
    void parseConversion();

    // Читання таблиці розбору. offset — на скільки записів заглянути вперед від поточного.
    const Token* tokenAt(std::size_t offset) const;  // nullptr — записи закінчилися
    bool atEnd() const { return numRow_ >= tokens_.size(); }
    bool is(std::size_t offset, TokenType type, std::string_view lexeme = {}) const;
    bool at(TokenType type, std::string_view lexeme = {}) const { return is(0, type, lexeme); }
    bool atKeyword(std::string_view word) const { return at(TokenType::Keyword, word); }
    bool atTypeKeyword(std::size_t offset) const;
    bool startsTypedDeclaration() const;

    // Перевірити, що поточний запис — термінал з токеном type (і лексемою lexeme, якщо задано),
    // та перейти до наступного запису. Інакше — синтаксична помилка.
    void parseToken(TokenType type, std::string_view lexeme = {});

    // Обробка помилок: запам'ятати діагностику й перервати розбір.
    [[noreturn]] void fail(const std::string& message);
    [[noreturn]] void failExpected(std::string_view expected);

    void traceLine(std::string_view text);

    const std::vector<Token>& tokens_;
    std::size_t numRow_ = 0;  // номер поточного запису таблиці розбору (з 0)
    std::optional<SyntaxError> error_;

    std::ostream* trace_ = nullptr;  // потік журналу розбору або nullptr
    int depth_ = 0;                  // глибина вкладеності викликів для відступів журналу
};

}  // namespace minigroovy
