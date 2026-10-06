#include "lexer/lexer.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include "lexer/char_class.hpp"
#include "lexer/token_table.hpp"

namespace minigroovy {

namespace {

// Найбільший допустимий цілий літерал. Це 2^31, а не 2^31 - 1, бо мінус
// не входить до літерала: запис -2147483648 — це унарний мінус і літерал
// 2147483648. Чи не стоїть такий літерал без мінуса, перевіряє вже
// семантичний аналіз.
constexpr unsigned long long kMaxIntLiteral = 2147483648ULL;

bool intLiteralInRange(const std::string& digits) {
    const std::size_t firstNonZero = digits.find_first_not_of('0');
    if (firstNonZero == std::string::npos) return true;  // лише нулі
    const std::string significant = digits.substr(firstNonZero);
    // Більше 10 значущих цифр точно не вміщаються; перевіряємо до stoull,
    // щоб не отримати виняток переповнення.
    if (significant.size() > 10) return false;
    return std::stoull(significant) <= kMaxIntLiteral;
}

bool floatLiteralInRange(const std::string& text) {
    // strtod при переповненні повертає нескінченність і встановлює ERANGE.
    // ERANGE буває і при надто малих числах (це не помилка), тому перевіряємо обидві умови.
    errno = 0;
    const double value = std::strtod(text.c_str(), nullptr);
    return !(errno == ERANGE && std::isinf(value));
}

// Довгий літерал у повідомленні про помилку скорочується, щоб не засмічувати вивід.
std::string shorten(const std::string& literal) {
    constexpr std::size_t kMaxShown = 40;
    if (literal.size() <= kMaxShown) return literal;
    return literal.substr(0, kMaxShown) + "...";
}

// Значення рядкової константи: без лапок і з розкодованими escape-послідовностями.
// Коректність escape-ів уже перевірив автомат (стан 18).
std::string decodeString(const std::string& lexeme) {
    std::string value;
    for (std::size_t i = 1; i + 1 < lexeme.size(); ++i) {
        if (lexeme[i] != '\\') {
            value += lexeme[i];
            continue;
        }
        switch (lexeme[++i]) {
            case 'n': value += '\n'; break;
            case 't': value += '\t'; break;
            case 'r': value += '\r'; break;
            default: value += lexeme[i]; break;  // лапка або зворотна скісна риска
        }
    }
    return value;
}

}  // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

LexResult Lexer::run() {
    State state = State::Start;
    while (true) {
        if (state == State::Start) {
            // Нова лексема починається з наступного символу.
            lexeme_.clear();
            lexemeStart_ = pos_;
        }

        const int ch = nextChar();                           // прочитати наступний символ
        state = nextState(state, ch, classOfChar(ch));       // обчислити наступний стан

        if (isFinal(state)) {
            if (!processing(state, ch)) break;               // виконати семантичні процедури
            state = State::Start;
        } else if (state != State::Start) {
            lexeme_ += static_cast<char>(ch);                // додати символ до лексеми
        }
    }
    return std::move(result_);
}

int Lexer::nextChar() {
    // Після кінця тексту позиція все одно зсувається, щоб putCharBack()
    // однаково працював і для звичайних символів, і для eof.
    const std::size_t pos = pos_++;
    if (pos >= source_.size()) return kEof;
    return static_cast<unsigned char>(source_[pos]);
}

void Lexer::putCharBack(int count) { pos_ -= static_cast<std::size_t>(count); }

bool Lexer::processing(State state, int ch) {
    // «Зірочка»: символи, що завершили лексему, повертаються у вхідний потік.
    putCharBack(charsToPutBack(state));

    switch (state) {
        case State::IdentEnd:
            // Повне слово прочитано; тепер визначаємо, чи воно зарезервоване.
            // Тому whileCount — ідентифікатор, а while — ключове слово.
            if (const auto info = lookupLexeme(lexeme_)) {
                addToken(info->type, info->code, 0);
            } else {
                addToken(TokenType::Id, token_code::kId, identifierIndex(lexeme_));
            }
            return true;

        case State::IntBeforeDot:
            lexeme_.pop_back();  // крапка повернута у вхідний потік і не належить числу
            return finishInteger(state);

        case State::IntEnd:
            return finishInteger(state);

        case State::FloatEnd:
            return finishFloat(state);

        case State::StringEnd:
            lexeme_ += static_cast<char>(ch);  // закривальна лапка
            finishString();
            return true;

        case State::TwoCharOp:
        case State::SingleChar:
            lexeme_ += static_cast<char>(ch);  // останній символ оператора
            addFixedToken();
            return true;

        case State::OneCharOpBack:
            addFixedToken();  // лексема — один уже прочитаний символ
            return true;

        case State::CommentEnd:
            return true;  // коментар у таблицю розбору не потрапляє

        case State::EndOfLine:
        case State::EndOfLineCr:
            ++line_;
            lineStart_ = pos_;
            return true;

        case State::EndOfFile:
            return false;  // успішне завершення

        default:
            reportError(state, ch);
            return false;  // аварійне завершення
    }
}

bool Lexer::finishInteger(State state) {
    if (!intLiteralInRange(lexeme_)) {
        fail(state, lexemeStart_, "integer literal " + shorten(lexeme_) + " is out of range (max 2147483648)");
        return false;
    }
    addToken(TokenType::IntNum, token_code::kIntNum, constantIndex(TokenType::IntNum, lexeme_));
    return true;
}

bool Lexer::finishFloat(State state) {
    if (!floatLiteralInRange(lexeme_)) {
        fail(state, lexemeStart_, "float literal " + shorten(lexeme_) + " is out of range");
        return false;
    }
    addToken(TokenType::FloatNum, token_code::kFloatNum, constantIndex(TokenType::FloatNum, lexeme_));
    return true;
}

void Lexer::finishString() {
    addToken(TokenType::StrVal, token_code::kStrVal, constantIndex(TokenType::StrVal, decodeString(lexeme_)));
}

void Lexer::addFixedToken() {
    // Оператори й розділювачі однозначно задаються написанням, тому їхній
    // токен береться з таблиці лексем. Відсутність лексеми в таблиці означала б
    // розбіжність таблиці переходів і таблиці лексем.
    const auto info = lookupLexeme(lexeme_);
    if (!info) throw std::logic_error("lexeme is missing from the token table: " + lexeme_);
    addToken(info->type, info->code, 0);
}

void Lexer::reportError(State state, int ch) {
    const std::size_t current = pos_ - 1;  // позиція символу, що спричинив помилку
    const std::string found = printableChar(source_, current);

    switch (state) {
        case State::ErrIllegalChar:
            if (lexeme_.starts_with("//")) {
                fail(state, current, "illegal character '" + found + "' in comment");
            } else if (lexeme_.starts_with('"')) {
                fail(state, current, "illegal character '" + found + "' in string literal");
            } else if (ch == '\\') {
                fail(state, current, "unexpected character '\\' outside a string literal");
            } else {
                fail(state, current, "illegal character '" + found + "'");
            }
            break;

        case State::ErrExpectedAmp:
            fail(state, lexemeStart_, "expected '&' after '&' but found '" + found + "' (single '&' is not an operator)");
            break;

        case State::ErrExpectedBar:
            fail(state, lexemeStart_, "expected '|' after '|' but found '" + found + "' (single '|' is not an operator)");
            break;

        case State::ErrBadEscape:
            // Позиція — зворотна скісна риска перед невідомим символом.
            fail(state, current - 1, "invalid escape sequence '\\" + found + "'");
            break;

        case State::ErrUnclosedString:
            fail(state, lexemeStart_, "unterminated string literal");
            break;

        case State::ErrTabInString:
            fail(state, current, "tab character inside a string literal (use \\t)");
            break;

        case State::ErrBadNumber: {
            // Для зрозумілого повідомлення дочитуємо «хвіст» літерала: 2value, 1e5.
            std::string literal = lexeme_;
            for (std::size_t i = current; i < source_.size(); ++i) {
                const CharClass cls = classOfChar(static_cast<unsigned char>(source_[i]));
                if (cls != CharClass::Letter && cls != CharClass::Digit && cls != CharClass::Dot) break;
                literal += source_[i];
            }
            fail(state, lexemeStart_, "invalid numeric literal '" + shorten(literal) + "'");
            break;
        }

        default:
            throw std::logic_error("processing() reached an unexpected state");
    }
}

void Lexer::addToken(TokenType type, int code, int index) {
    result_.tokens.push_back(Token{line_, columnAt(lexemeStart_), lexeme_, type, code, index});
}

int Lexer::identifierIndex(const std::string& name) {
    // Обробка таблиці ідентифікаторів: повторне входження отримує той самий індекс.
    const auto [it, inserted] = identifierIndex_.try_emplace(name, static_cast<int>(result_.identifiers.size()) + 1);
    if (inserted) result_.identifiers.push_back(name);
    return it->second;
}

int Lexer::constantIndex(TokenType type, const std::string& value) {
    // Обробка таблиці констант: ключ — лексема, бо за нею однозначно визначається тип.
    const auto [it, inserted] = constantIndex_.try_emplace(lexeme_, static_cast<int>(result_.constants.size()) + 1);
    if (inserted) result_.constants.push_back(ConstantEntry{lexeme_, type, value, it->second});
    return it->second;
}

void Lexer::fail(State state, std::size_t errorPos, std::string message) {
    result_.error = LexicalError{line_, columnAt(errorPos), state, std::move(message)};
}

int Lexer::columnAt(std::size_t pos) const { return countCodePoints(source_, lineStart_, pos) + 1; }

}  // namespace minigroovy
