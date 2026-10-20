#include "parser/parser.hpp"

#include <format>
#include <string>

namespace minigroovy {

namespace {

// Виняток, яким fail() перериває рекурсивний спуск (аналог SystemExit у прикладі
// з методичних матеріалів). Діагностику на цей момент уже записано в error_.
struct SyntaxFailure {};

constexpr int kTraceIndent = 2;  // крок відступу журналу розбору

// Назва категорії лексем для повідомлення «expected ...».
std::string_view describeTokenType(TokenType type) {
    switch (type) {
        case TokenType::Id: return "identifier";
        case TokenType::IntNum: return "integer constant";
        case TokenType::FloatNum: return "float constant";
        case TokenType::StrVal: return "string constant";
        case TokenType::BoolVal: return "boolean constant";
        default: return tokenTypeName(type);
    }
}

}  // namespace

// Рядок журналу «parseName():» на вході в метод розбору та збільшення відступу;
// відступ зменшується автоматично на виході з методу, зокрема під час аварійного завершення.
class TraceScope {
public:
    TraceScope(Parser& parser, std::string_view name) : parser_(parser) {
        if (parser_.trace_) parser_.traceLine(std::string(name) + "():");
        ++parser_.depth_;
    }
    ~TraceScope() { --parser_.depth_; }

    TraceScope(const TraceScope&) = delete;
    TraceScope& operator=(const TraceScope&) = delete;

private:
    Parser& parser_;
};

Parser::Parser(const std::vector<Token>& tokens, std::ostream* trace) : tokens_(tokens), trace_(trace) {}

ParseResult Parser::run() {
    numRow_ = 0;
    error_.reset();
    try {
        parseProgram();
    } catch (const SyntaxFailure&) {
        // Діагностику записано в error_ функцією fail().
    }
    return ParseResult{error_};
}

// ---------------------------------------------------------------------------
// Читання таблиці розбору
// ---------------------------------------------------------------------------

const Token* Parser::tokenAt(std::size_t offset) const {
    const std::size_t row = numRow_ + offset;
    return row < tokens_.size() ? &tokens_[row] : nullptr;
}

bool Parser::is(std::size_t offset, TokenType type, std::string_view lexeme) const {
    const Token* token = tokenAt(offset);
    return token != nullptr && token->type == type && (lexeme.empty() || token->lexeme == lexeme);
}

bool Parser::atTypeKeyword(std::size_t offset) const {
    return is(offset, TokenType::Keyword, "int") || is(offset, TokenType::Keyword, "float") ||
           is(offset, TokenType::Keyword, "boolean") || is(offset, TokenType::Keyword, "String");
}

// Чи починається з поточного запису оголошення з типом: Type Ident ...
// Type = 'int' | 'float' | 'boolean' | 'String' | TypeName, де TypeName = Ident.
// Оголошення з TypeName і присвоєння/виклик починаються однаково — з Ident,
// тому заглядаємо на запис уперед: «Ident Ident» може бути лише оголошенням.
bool Parser::startsTypedDeclaration() const {
    return atTypeKeyword(0) || (at(TokenType::Id) && is(1, TokenType::Id));
}

void Parser::parseToken(TokenType type, std::string_view lexeme) {
    std::string expected;
    if (lexeme.empty()) {
        expected = describeTokenType(type);
    } else {
        expected = std::format("'{}'", lexeme);
    }
    if (atEnd()) failExpected(expected);

    const Token& token = tokens_[numRow_];
    if (token.type != type || (!lexeme.empty() && token.lexeme != lexeme)) failExpected(expected);

    if (trace_) {
        traceLine(std::format("parseToken: line {}, token ('{}', {})", token.line, token.lexeme,
                              tokenTypeName(token.type)));
    }
    ++numRow_;
}

// ---------------------------------------------------------------------------
// Обробка помилок і журнал
// ---------------------------------------------------------------------------

void Parser::fail(const std::string& message) {
    if (const Token* token = tokenAt(0)) {
        error_ = SyntaxError{token->line, token->column, message};
    } else if (!tokens_.empty()) {
        // Записи закінчилися: помилка — одразу за останньою лексемою програми.
        const Token& last = tokens_.back();
        error_ = SyntaxError{last.line, last.column + static_cast<int>(last.lexeme.size()), message};
    } else {
        error_ = SyntaxError{1, 1, message};
    }
    throw SyntaxFailure{};
}

void Parser::failExpected(std::string_view expected) {
    if (const Token* token = tokenAt(0)) {
        fail(std::format("expected {} but found '{}' ({})", expected, token->lexeme, tokenTypeName(token->type)));
    }
    fail(std::format("unexpected end of program, expected {}", expected));
}

void Parser::traceLine(std::string_view text) {
    *trace_ << std::string(static_cast<std::size_t>(depth_ * kTraceIndent), ' ') << text << '\n';
}

// ---------------------------------------------------------------------------
// 7.1 Синтаксичні правила високого рівня
// ---------------------------------------------------------------------------

// Program = { GlobalItem } .
void Parser::parseProgram() {
    TraceScope scope(*this, "parseProgram");
    while (!atEnd()) parseGlobalItem();
}

// GlobalItem = Declaration | FunctionDecl | Statement .
void Parser::parseGlobalItem() {
    TraceScope scope(*this, "parseGlobalItem");
    if (atKeyword("void")) {
        parseFunctionDecl();
    } else if (atKeyword("final") || atKeyword("enum") || atKeyword("type")) {
        parseDeclaration();
    } else if (startsTypedDeclaration()) {
        // Type Ident '(' — функція, інакше — оголошення змінної чи масиву.
        if (is(2, TokenType::Paren, "(")) {
            parseFunctionDecl();
        } else {
            parseDeclaration();
        }
    } else {
        parseStatement();
    }
}

// Declaration = VarDeclaration | ConstDeclaration | ArrayDeclaration
//             | EnumDeclaration | TypeDeclaration .
void Parser::parseDeclaration() {
    TraceScope scope(*this, "parseDeclaration");
    if (atKeyword("enum")) {
        parseEnumDeclaration();
    } else if (atKeyword("type")) {
        parseTypeDeclaration();
    } else {
        parseLocalDeclaration();
    }
}

// LocalDeclaration = VarDeclaration | ConstDeclaration | ArrayDeclaration .
// VarDeclaration і ArrayDeclaration мають спільний початок Type Ident,
// їх розрізняє третій запис: '[' — масив.
void Parser::parseLocalDeclaration() {
    TraceScope scope(*this, "parseLocalDeclaration");
    if (atKeyword("final")) {
        parseConstDeclaration();
    } else if (is(2, TokenType::Bracket, "[")) {
        parseArrayDeclaration();
    } else {
        parseVarDeclaration();
    }
}

// VarDeclaration = Type Ident [ '=' Expression ] ';' .
void Parser::parseVarDeclaration() {
    TraceScope scope(*this, "parseVarDeclaration");
    parseType();
    parseToken(TokenType::Id);
    if (at(TokenType::AssignOp)) {
        parseToken(TokenType::AssignOp, "=");
        parseExpression();
    }
    parseToken(TokenType::Punct, ";");
}

// ConstDeclaration = 'final' Type Ident '=' Expression ';' .
void Parser::parseConstDeclaration() {
    TraceScope scope(*this, "parseConstDeclaration");
    parseToken(TokenType::Keyword, "final");
    parseType();
    parseToken(TokenType::Id);
    parseToken(TokenType::AssignOp, "=");
    parseExpression();
    parseToken(TokenType::Punct, ";");
}

// ArrayDeclaration = Type Ident '[' ArraySize ']' [ '=' '{' InitList '}' ] ';' .
void Parser::parseArrayDeclaration() {
    TraceScope scope(*this, "parseArrayDeclaration");
    parseType();
    parseToken(TokenType::Id);
    parseToken(TokenType::Bracket, "[");
    parseArraySize();
    parseToken(TokenType::Bracket, "]");
    if (at(TokenType::AssignOp)) {
        parseToken(TokenType::AssignOp, "=");
        parseToken(TokenType::Brace, "{");
        parseInitList();
        parseToken(TokenType::Brace, "}");
    }
    parseToken(TokenType::Punct, ";");
}

// EnumDeclaration = 'enum' Ident '{' EnumItems '}' .
void Parser::parseEnumDeclaration() {
    TraceScope scope(*this, "parseEnumDeclaration");
    parseToken(TokenType::Keyword, "enum");
    parseToken(TokenType::Id);
    parseToken(TokenType::Brace, "{");
    parseEnumItems();
    parseToken(TokenType::Brace, "}");
}

// TypeDeclaration = 'type' Ident '=' RangeBound '..' RangeBound ';' .
void Parser::parseTypeDeclaration() {
    TraceScope scope(*this, "parseTypeDeclaration");
    parseToken(TokenType::Keyword, "type");
    parseToken(TokenType::Id);
    parseToken(TokenType::AssignOp, "=");
    parseRangeBound();
    parseToken(TokenType::RangeOp, "..");
    parseRangeBound();
    parseToken(TokenType::Punct, ";");
}

// ArraySize = IntConst | Ident .
void Parser::parseArraySize() {
    TraceScope scope(*this, "parseArraySize");
    if (at(TokenType::IntNum)) {
        parseToken(TokenType::IntNum);
    } else if (at(TokenType::Id)) {
        parseToken(TokenType::Id);
    } else {
        failExpected("array size (integer constant or identifier)");
    }
}

// InitList = Expression { ',' Expression } .
void Parser::parseInitList() {
    TraceScope scope(*this, "parseInitList");
    parseExpression();
    while (at(TokenType::Punct, ",")) {
        parseToken(TokenType::Punct, ",");
        parseExpression();
    }
}

// EnumItems = Ident { ',' Ident } .
void Parser::parseEnumItems() {
    TraceScope scope(*this, "parseEnumItems");
    parseToken(TokenType::Id);
    while (at(TokenType::Punct, ",")) {
        parseToken(TokenType::Punct, ",");
        parseToken(TokenType::Id);
    }
}

// RangeBound = [ '-' ] IntConst | Ident .
void Parser::parseRangeBound() {
    TraceScope scope(*this, "parseRangeBound");
    if (at(TokenType::Id)) {
        parseToken(TokenType::Id);
        return;
    }
    if (at(TokenType::AddOp, "-")) parseToken(TokenType::AddOp, "-");
    if (!at(TokenType::IntNum)) failExpected("range bound (integer constant or identifier)");
    parseToken(TokenType::IntNum);
}

// Type = 'int' | 'float' | 'boolean' | 'String' | TypeName .
// TypeName = Ident .
void Parser::parseType() {
    TraceScope scope(*this, "parseType");
    if (atTypeKeyword(0)) {
        parseToken(TokenType::Keyword);
    } else if (at(TokenType::Id)) {
        parseToken(TokenType::Id);
    } else {
        failExpected("type");
    }
}

// FunctionDecl = ReturnType Ident '(' [ ParamList ] ')' Block .
void Parser::parseFunctionDecl() {
    TraceScope scope(*this, "parseFunctionDecl");
    parseReturnType();
    parseToken(TokenType::Id);
    parseToken(TokenType::Paren, "(");
    if (!at(TokenType::Paren, ")")) parseParamList();
    parseToken(TokenType::Paren, ")");
    parseBlock();
}

// ReturnType = Type | 'void' .
void Parser::parseReturnType() {
    TraceScope scope(*this, "parseReturnType");
    if (atKeyword("void")) {
        parseToken(TokenType::Keyword, "void");
    } else {
        parseType();
    }
}

// ParamList = Parameter { ',' Parameter } .
void Parser::parseParamList() {
    TraceScope scope(*this, "parseParamList");
    parseParameter();
    while (at(TokenType::Punct, ",")) {
        parseToken(TokenType::Punct, ",");
        parseParameter();
    }
}

// Parameter = Type Ident [ '[' ']' ] .
void Parser::parseParameter() {
    TraceScope scope(*this, "parseParameter");
    parseType();
    parseToken(TokenType::Id);
    if (at(TokenType::Bracket, "[")) {
        parseToken(TokenType::Bracket, "[");
        parseToken(TokenType::Bracket, "]");
    }
}

// ---------------------------------------------------------------------------
// 7.2 Інструкції та керуючі конструкції
// ---------------------------------------------------------------------------

// StatementList = { BlockItem } .
// Список закінчується перед '}' (кінець блоку), перед 'case'/'default'
// (наступна секція switch) або в кінці таблиці розбору.
void Parser::parseStatementList() {
    TraceScope scope(*this, "parseStatementList");
    while (!atEnd() && !at(TokenType::Brace, "}") && !atKeyword("case") && !atKeyword("default")) {
        parseBlockItem();
    }
}

// BlockItem = LocalDeclaration | Statement .
void Parser::parseBlockItem() {
    TraceScope scope(*this, "parseBlockItem");
    // Функції, перелічення й діапазонні типи оголошуються лише на глобальному рівні.
    if (atKeyword("enum") || atKeyword("type")) {
        fail(std::format("'{}' declarations are allowed only at the global level", tokens_[numRow_].lexeme));
    }
    if (atKeyword("void") || (startsTypedDeclaration() && is(2, TokenType::Paren, "("))) {
        fail("function declarations are allowed only at the global level");
    }
    if (atKeyword("final") || startsTypedDeclaration()) {
        parseLocalDeclaration();
    } else {
        parseStatement();
    }
}

// Statement = Assignment | Output | IfStatement | SwitchStatement | WhileStatement
//           | ForStatement | CallStatement | ReturnStatement | BreakStatement | Block .
// Альтернативу визначає поточний запис; для Ident — ще й наступний: '(' — виклик функції.
void Parser::parseStatement() {
    TraceScope scope(*this, "parseStatement");
    if (at(TokenType::Id)) {
        if (is(1, TokenType::Paren, "(")) {
            parseCallStatement();
        } else {
            parseAssignment();
        }
    } else if (at(TokenType::Builtin, "print")) {
        parseOutput();
    } else if (atKeyword("if")) {
        parseIf();
    } else if (atKeyword("switch")) {
        parseSwitch();
    } else if (atKeyword("while")) {
        parseWhile();
    } else if (atKeyword("for")) {
        parseFor();
    } else if (atKeyword("return")) {
        parseReturn();
    } else if (atKeyword("break")) {
        parseBreak();
    } else if (at(TokenType::Brace, "{")) {
        parseBlock();
    } else {
        failExpected("statement");
    }
}

// Block = '{' StatementList '}' .
void Parser::parseBlock() {
    TraceScope scope(*this, "parseBlock");
    parseToken(TokenType::Brace, "{");
    parseStatementList();
    parseToken(TokenType::Brace, "}");
}

// Assignment = Target '=' Expression ';' .
void Parser::parseAssignment() {
    TraceScope scope(*this, "parseAssignment");
    parseTarget();
    parseToken(TokenType::AssignOp, "=");
    parseExpression();
    parseToken(TokenType::Punct, ";");
}

// Target = Ident [ '[' Expression ']' ] .
void Parser::parseTarget() {
    TraceScope scope(*this, "parseTarget");
    parseToken(TokenType::Id);
    if (at(TokenType::Bracket, "[")) {
        parseToken(TokenType::Bracket, "[");
        parseExpression();
        parseToken(TokenType::Bracket, "]");
    }
}

// Output = 'print' '(' Expression ')' ';' .
void Parser::parseOutput() {
    TraceScope scope(*this, "parseOutput");
    parseToken(TokenType::Builtin, "print");
    parseToken(TokenType::Paren, "(");
    parseExpression();
    parseToken(TokenType::Paren, ")");
    parseToken(TokenType::Punct, ";");
}

// IfStatement = 'if' '(' Expression ')' Block [ 'else' Block ] .
void Parser::parseIf() {
    TraceScope scope(*this, "parseIf");
    parseToken(TokenType::Keyword, "if");
    parseToken(TokenType::Paren, "(");
    parseExpression();
    parseToken(TokenType::Paren, ")");
    parseBlock();
    if (atKeyword("else")) {
        parseToken(TokenType::Keyword, "else");
        parseBlock();
    }
}

// SwitchStatement = 'switch' '(' Expression ')' '{' { SwitchSection } '}' .
void Parser::parseSwitch() {
    TraceScope scope(*this, "parseSwitch");
    parseToken(TokenType::Keyword, "switch");
    parseToken(TokenType::Paren, "(");
    parseExpression();
    parseToken(TokenType::Paren, ")");
    parseToken(TokenType::Brace, "{");
    while (!at(TokenType::Brace, "}")) {
        if (!atKeyword("case") && !atKeyword("default")) failExpected("'case', 'default' or '}'");
        parseSwitchSection();
    }
    parseToken(TokenType::Brace, "}");
}

// SwitchSection = SwitchLabel { SwitchLabel } StatementList .
void Parser::parseSwitchSection() {
    TraceScope scope(*this, "parseSwitchSection");
    parseSwitchLabel();
    while (atKeyword("case") || atKeyword("default")) parseSwitchLabel();
    parseStatementList();
}

// SwitchLabel = 'case' CaseValue ':' | 'default' ':' .
void Parser::parseSwitchLabel() {
    TraceScope scope(*this, "parseSwitchLabel");
    if (atKeyword("default")) {
        parseToken(TokenType::Keyword, "default");
    } else {
        parseToken(TokenType::Keyword, "case");
        parseCaseValue();
    }
    parseToken(TokenType::Punct, ":");
}

// CaseValue = [ '-' ] IntConst | StringConst | Ident [ '.' Ident ] .
void Parser::parseCaseValue() {
    TraceScope scope(*this, "parseCaseValue");
    if (at(TokenType::StrVal)) {
        parseToken(TokenType::StrVal);
    } else if (at(TokenType::Id)) {
        parseToken(TokenType::Id);
        if (at(TokenType::Punct, ".")) {
            parseToken(TokenType::Punct, ".");
            parseToken(TokenType::Id);
        }
    } else if (at(TokenType::AddOp, "-") || at(TokenType::IntNum)) {
        if (at(TokenType::AddOp, "-")) parseToken(TokenType::AddOp, "-");
        parseToken(TokenType::IntNum);
    } else {
        failExpected("case value (integer, string or enumeration constant)");
    }
}

// WhileStatement = 'while' '(' Expression ')' Block .
void Parser::parseWhile() {
    TraceScope scope(*this, "parseWhile");
    parseToken(TokenType::Keyword, "while");
    parseToken(TokenType::Paren, "(");
    parseExpression();
    parseToken(TokenType::Paren, ")");
    parseBlock();
}

// ForStatement = 'for' '(' [ ForInit ] ';' [ Expression ] ';' [ ForUpdate ] ')' Block .
void Parser::parseFor() {
    TraceScope scope(*this, "parseFor");
    parseToken(TokenType::Keyword, "for");
    parseToken(TokenType::Paren, "(");
    if (!at(TokenType::Punct, ";")) parseForInit();
    parseToken(TokenType::Punct, ";");
    if (!at(TokenType::Punct, ";")) parseExpression();
    parseToken(TokenType::Punct, ";");
    if (!at(TokenType::Paren, ")")) parseForUpdate();
    parseToken(TokenType::Paren, ")");
    parseBlock();
}

// ForInit = ForVarDecl | Target '=' Expression .
void Parser::parseForInit() {
    TraceScope scope(*this, "parseForInit");
    if (startsTypedDeclaration()) {
        parseForVarDecl();
    } else {
        parseTarget();
        parseToken(TokenType::AssignOp, "=");
        parseExpression();
    }
}

// ForVarDecl = Type Ident [ '=' Expression ] .
void Parser::parseForVarDecl() {
    TraceScope scope(*this, "parseForVarDecl");
    parseType();
    parseToken(TokenType::Id);
    if (at(TokenType::AssignOp)) {
        parseToken(TokenType::AssignOp, "=");
        parseExpression();
    }
}

// ForUpdate = Target '=' Expression | FunctionCall .
void Parser::parseForUpdate() {
    TraceScope scope(*this, "parseForUpdate");
    if (at(TokenType::Id) && is(1, TokenType::Paren, "(")) {
        parseFunctionCall();
    } else {
        parseTarget();
        parseToken(TokenType::AssignOp, "=");
        parseExpression();
    }
}

// CallStatement = FunctionCall ';' .
void Parser::parseCallStatement() {
    TraceScope scope(*this, "parseCallStatement");
    parseFunctionCall();
    parseToken(TokenType::Punct, ";");
}

// ReturnStatement = 'return' [ Expression ] ';' .
void Parser::parseReturn() {
    TraceScope scope(*this, "parseReturn");
    parseToken(TokenType::Keyword, "return");
    if (!at(TokenType::Punct, ";")) parseExpression();
    parseToken(TokenType::Punct, ";");
}

// BreakStatement = 'break' ';' .
void Parser::parseBreak() {
    TraceScope scope(*this, "parseBreak");
    parseToken(TokenType::Keyword, "break");
    parseToken(TokenType::Punct, ";");
}

// FunctionCall = Ident '(' [ Arguments ] ')' .
void Parser::parseFunctionCall() {
    TraceScope scope(*this, "parseFunctionCall");
    parseToken(TokenType::Id);
    parseToken(TokenType::Paren, "(");
    if (!at(TokenType::Paren, ")")) parseArguments();
    parseToken(TokenType::Paren, ")");
}

// Arguments = Expression { ',' Expression } .
void Parser::parseArguments() {
    TraceScope scope(*this, "parseArguments");
    parseExpression();
    while (at(TokenType::Punct, ",")) {
        parseToken(TokenType::Punct, ",");
        parseExpression();
    }
}

// ---------------------------------------------------------------------------
// 7.3 Вирази: кожен рівень пріоритету — окремий метод; ліва рекурсія
// замінена повторенням { ... }, тому ліва асоціативність дає цикл while.
// ---------------------------------------------------------------------------

// Expression = OrExpr .
void Parser::parseExpression() {
    TraceScope scope(*this, "parseExpression");
    parseOrExpr();
}

// OrExpr = AndExpr { '||' AndExpr } .
void Parser::parseOrExpr() {
    TraceScope scope(*this, "parseOrExpr");
    parseAndExpr();
    while (at(TokenType::OrOp)) {
        parseToken(TokenType::OrOp, "||");
        parseAndExpr();
    }
}

// AndExpr = EqualityExpr { '&&' EqualityExpr } .
void Parser::parseAndExpr() {
    TraceScope scope(*this, "parseAndExpr");
    parseEqualityExpr();
    while (at(TokenType::AndOp)) {
        parseToken(TokenType::AndOp, "&&");
        parseEqualityExpr();
    }
}

// EqualityExpr = RelExpr [ EqualityOp RelExpr ] .
// Оператор необов'язковий і не повторюється: a == b == c — синтаксична помилка.
void Parser::parseEqualityExpr() {
    TraceScope scope(*this, "parseEqualityExpr");
    parseRelExpr();
    if (at(TokenType::RelOp, "==") || at(TokenType::RelOp, "!=")) {
        parseToken(TokenType::RelOp);
        parseRelExpr();
        if (at(TokenType::RelOp, "==") || at(TokenType::RelOp, "!=")) {
            fail("equality operators are non-associative, use parentheses");
        }
    }
}

// RelExpr = AddExpr [ RelOp AddExpr ] .
// Так само неасоціативний: a < b < c — синтаксична помилка.
void Parser::parseRelExpr() {
    TraceScope scope(*this, "parseRelExpr");
    const auto atRelOp = [this] {
        return at(TokenType::RelOp, "<") || at(TokenType::RelOp, "<=") || at(TokenType::RelOp, ">") ||
               at(TokenType::RelOp, ">=");
    };
    parseAddExpr();
    if (atRelOp()) {
        parseToken(TokenType::RelOp);
        parseAddExpr();
        if (atRelOp()) fail("relational operators are non-associative, use parentheses");
    }
}

// AddExpr = Term { ( '+' | '-' ) Term } .
void Parser::parseAddExpr() {
    TraceScope scope(*this, "parseAddExpr");
    parseTerm();
    while (at(TokenType::AddOp)) {
        parseToken(TokenType::AddOp);
        parseTerm();
    }
}

// Term = UnaryExpr { ( '*' | '/' ) UnaryExpr } .
void Parser::parseTerm() {
    TraceScope scope(*this, "parseTerm");
    parseUnaryExpr();
    while (at(TokenType::MultOp)) {
        parseToken(TokenType::MultOp);
        parseUnaryExpr();
    }
}

// UnaryExpr = '-' UnaryExpr | '!' UnaryExpr | PowerExpr .
void Parser::parseUnaryExpr() {
    TraceScope scope(*this, "parseUnaryExpr");
    if (at(TokenType::AddOp, "-")) {
        parseToken(TokenType::AddOp, "-");
        parseUnaryExpr();
    } else if (at(TokenType::NotOp)) {
        parseToken(TokenType::NotOp, "!");
        parseUnaryExpr();
    } else {
        parsePowerExpr();
    }
}

// PowerExpr = Primary [ '**' UnaryExpr ] .
// Правий операнд — знову UnaryExpr (а через нього PowerExpr), тому
// 2 ** 3 ** 2 розбирається як 2 ** (3 ** 2): права асоціативність.
void Parser::parsePowerExpr() {
    TraceScope scope(*this, "parsePowerExpr");
    parsePrimary();
    if (at(TokenType::PowOp)) {
        parseToken(TokenType::PowOp, "**");
        parseUnaryExpr();
    }
}

// Primary = Const | NameExpr | ReadCall | Conversion | '(' Expression ')' .
// Const = IntConst | FloatConst | BoolConst | StringConst .
void Parser::parsePrimary() {
    TraceScope scope(*this, "parsePrimary");
    if (at(TokenType::IntNum) || at(TokenType::FloatNum) || at(TokenType::StrVal) || at(TokenType::BoolVal)) {
        parseToken(tokens_[numRow_].type);
    } else if (at(TokenType::Id)) {
        parseNameExpr();
    } else if (at(TokenType::Builtin, "read")) {
        parseReadCall();
    } else if (atKeyword("int") || atKeyword("float")) {
        parseConversion();
    } else if (at(TokenType::Paren, "(")) {
        parseToken(TokenType::Paren, "(");
        parseExpression();
        parseToken(TokenType::Paren, ")");
    } else {
        failExpected("expression");
    }
}

// NameExpr = Ident [ '[' Expression ']' | '.' Ident | '(' [ Arguments ] ')' ] .
void Parser::parseNameExpr() {
    TraceScope scope(*this, "parseNameExpr");
    parseToken(TokenType::Id);
    if (at(TokenType::Bracket, "[")) {
        parseToken(TokenType::Bracket, "[");
        parseExpression();
        parseToken(TokenType::Bracket, "]");
    } else if (at(TokenType::Punct, ".")) {
        parseToken(TokenType::Punct, ".");
        parseToken(TokenType::Id);
    } else if (at(TokenType::Paren, "(")) {
        parseToken(TokenType::Paren, "(");
        if (!at(TokenType::Paren, ")")) parseArguments();
        parseToken(TokenType::Paren, ")");
    }
}

// ReadCall = 'read' '(' ')' .
void Parser::parseReadCall() {
    TraceScope scope(*this, "parseReadCall");
    parseToken(TokenType::Builtin, "read");
    parseToken(TokenType::Paren, "(");
    parseToken(TokenType::Paren, ")");
}

// Conversion = ( 'int' | 'float' ) '(' Expression ')' .
void Parser::parseConversion() {
    TraceScope scope(*this, "parseConversion");
    parseToken(TokenType::Keyword);
    parseToken(TokenType::Paren, "(");
    parseExpression();
    parseToken(TokenType::Paren, ")");
}

}  // namespace minigroovy
