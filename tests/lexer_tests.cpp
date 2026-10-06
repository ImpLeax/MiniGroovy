// Модульні тести лексичного аналізатора.
// Більшість випадків перенесено з першої версії лексера та доповнено
// діапазонними типами (type, ..) і правилом «зупинка на першій помилці».

#include "lexer_tests.hpp"

#include <format>
#include <string>

#include "lexer/lexer.hpp"

namespace minigroovy::test {

namespace {

LexResult lex(const std::string& source) { return Lexer(source).run(); }

// Коди табл. 2 усіх розпізнаних лексем через пробіл, напр. "1 21 2 42".
std::string codesOf(const LexResult& result) {
    std::string codes;
    for (const Token& token : result.tokens) {
        if (!codes.empty()) codes += ' ';
        codes += std::to_string(token.code);
    }
    return codes;
}

std::string describe(const LexResult& result) {
    std::string text = "codes [" + codesOf(result) + "]";
    if (result.error) {
        text += std::format(", error in state {} at {}:{}: {}", static_cast<int>(result.error->state),
                            result.error->line, result.error->column, result.error->message);
    }
    return text;
}

// Аналіз має завершитися успішно з указаною послідовністю кодів.
void expectTokens(TestRunner& t, const std::string& name, const std::string& source, const std::string& codes) {
    const LexResult result = lex(source);
    t.check(result.ok() && codesOf(result) == codes, name, "expected codes [" + codes + "], got " + describe(result));
}

// Аналіз має аварійно завершитися в стані state; до помилки — лексеми codesBefore.
void expectError(TestRunner& t, const std::string& name, const std::string& source, const std::string& codesBefore,
                 State state) {
    const LexResult result = lex(source);
    const bool ok = result.error && result.error->state == state && codesOf(result) == codesBefore;
    t.check(ok, name,
            std::format("expected state {} after [{}], got {}", static_cast<int>(state), codesBefore, describe(result)));
}

void testValidInput(TestRunner& t) {
    expectTokens(t, "keywords vs identifiers", "while whileCount _ String string", "16 1 1 10 1");
    expectTokens(t, "builtins and booleans", "print read true false", "20 19 5 6");
    expectTokens(t, "all keywords", "int float boolean String void final enum type if else while for return",
                 "7 8 9 10 11 12 13 51 14 15 16 17 18");
    expectTokens(t, "leading zeros", "007", "2");
    expectTokens(t, "float literals", "0.5 12.0 3.14", "3 3 3");
    expectTokens(t, "12. is int + dot", "12.", "2 44");
    expectTokens(t, ".5 is dot + int", ".5", "44 2");
    expectTokens(t, "12.x is int + dot + id", "12.x", "2 44 1");
    expectTokens(t, "minus is a separate token", "-12", "23 2");
    expectTokens(t, "power vs multiplication", "a**b*c", "1 26 1 24 1");
    expectTokens(t, "comparison operators", "< <= > >= == != =", "27 28 29 30 31 32 21");
    expectTokens(t, "logic operators", "&& || !", "33 34 35");
    expectTokens(t, "a = b == c", "a = b == c", "1 21 1 31 1");
    expectTokens(t, "separated stars are two tokens", "* *", "24 24");
    expectTokens(t, "separated assigns are two tokens", "= =", "21 21");
    expectTokens(t, "brackets and punctuation", "( ) { } [ ] ; , .", "36 37 38 39 40 41 42 43 44");
    expectTokens(t, "enum access", "State.ON", "1 44 1");
    expectTokens(t, "conversion call", "int(read())", "7 36 19 36 37 37");
    expectTokens(t, "division vs comment", "a / b // c", "1 25 1");
    expectTokens(t, "comment is skipped", "a // comment ( ) ; \"\n b", "1 1");
    expectTokens(t, "comment at end of file", "a // end", "1");
    expectTokens(t, "tab inside comment", "// a\tb", "");
    expectTokens(t, "// inside string", "\"a // b\"", "4");
    expectTokens(t, "empty string", "\"\"", "4");
    expectTokens(t, "empty input", "", "");
    expectTokens(t, "max int literal for unary minus", "2147483648", "2");
    // Діапазонні типи
    expectTokens(t, "range type declaration", "type Percent = 0..100;", "51 1 21 2 52 2 42");
    expectTokens(t, "range with spaces", "0 .. 100", "2 52 2");
    expectTokens(t, "range with identifiers", "LOW..HIGH", "1 52 1");
    expectTokens(t, "range with negative bound", "-5..5", "23 2 52 2");
    expectTokens(t, "three dots", "...", "52 44");
}

void testErrors(TestRunner& t) {
    expectError(t, "illegal character #", "x # y", "1", State::ErrIllegalChar);
    expectError(t, "illegal character '", "a ' b", "1", State::ErrIllegalChar);
    expectError(t, "colon is not in the alphabet", "a : b", "1", State::ErrIllegalChar);
    expectError(t, "backslash outside string", "a \\ b", "1", State::ErrIllegalChar);
    expectError(t, "illegal character in comment", "// ok ? \nx", "", State::ErrIllegalChar);
    expectError(t, "cyrillic in comment", "// \xD0\xBF\xD1\x80\xD0\xB8", "", State::ErrIllegalChar);
    expectError(t, "cyrillic outside string", "\xD1\x8F", "", State::ErrIllegalChar);
    expectError(t, "illegal character in string", "\"a#b\"", "", State::ErrIllegalChar);
    expectError(t, "single &", "a & b", "1", State::ErrExpectedAmp);
    expectError(t, "single | at end of file", "a |", "1", State::ErrExpectedBar);
    expectError(t, "invalid escape", "\"a\\qb\"", "", State::ErrBadEscape);
    expectError(t, "backslash at end of file", "\"a\\", "", State::ErrBadEscape);
    expectError(t, "unterminated string at end of file", "\"abc", "", State::ErrUnclosedString);
    expectError(t, "unterminated string at LF", "x = \"abc\nx", "1 21", State::ErrUnclosedString);
    expectError(t, "unterminated string at CRLF", "\"abc\r\nx", "", State::ErrUnclosedString);
    expectError(t, "escaped quote then end of file", "\"a\\\"", "", State::ErrUnclosedString);
    expectError(t, "raw tab in string", "\"a\tb\"", "", State::ErrTabInString);
    expectError(t, "2value", "2value", "", State::ErrBadNumber);
    expectError(t, "1e5", "1e5", "", State::ErrBadNumber);
    expectError(t, "10f", "x = 10f;", "1 21", State::ErrBadNumber);
    expectError(t, "3.5abc", "3.5abc", "", State::ErrBadNumber);
    expectError(t, "int literal too large", "99999999999999999999", "", State::IntEnd);
    expectError(t, "int literal 2147483649", "2147483649", "", State::IntEnd);
    expectError(t, "int literal too large before dot", "2147483649.", "", State::IntBeforeDot);
    expectError(t, "float literal too large", "1" + std::string(400, '0') + ".0", "", State::FloatEnd);
}

void testValuesAndPositions(TestRunner& t) {
    {
        const LexResult r = lex("\"Line1\\nLine2 \\\"q\\\" \\\\ \\t\"");
        const bool ok = r.ok() && r.constants.size() == 1;
        t.check(ok && r.constants[0].value == "Line1\nLine2 \"q\" \\ \t", "string value is decoded");
        t.check(ok && r.tokens[0].lexeme == "\"Line1\\nLine2 \\\"q\\\" \\\\ \\t\"", "raw lexeme is preserved");
    }
    {
        const LexResult r = lex("a\r\nb");
        t.check(r.ok() && r.tokens.size() == 2 && r.tokens[1].line == 2 && r.tokens[1].column == 1,
                "CRLF is one line break");
    }
    {
        const LexResult r = lex("a\rb\nc");
        t.check(r.ok() && r.tokens.size() == 3 && r.tokens[1].line == 2 && r.tokens[2].line == 3,
                "lone CR and lone LF are line breaks");
    }
    {
        const LexResult r = lex("a\n  bc");
        t.check(r.ok() && r.tokens.size() == 2 && r.tokens[1].line == 2 && r.tokens[1].column == 3,
                "token position");
    }
    {
        const LexResult r = lex("12.");
        t.check(r.ok() && r.tokens.size() == 2 && r.tokens[1].lexeme == "." && r.tokens[1].column == 3,
                "dot is returned to the input after '12.'");
    }
    {
        const LexResult r = lex("a\n\"x\ny");
        t.check(r.error && r.error->line == 2 && r.error->column == 1,
                "unterminated string error points at the opening quote");
    }
    {
        // "я" — 2 байти UTF-8, але один символ: стовпці рахуються в символах, а не в байтах.
        const LexResult r = lex("a\xD1\x8F#");
        t.check(r.error && r.error->column == 2, "column of an illegal UTF-8 character");
        const LexResult r2 = lex("// \xD1\x8F#");
        t.check(r2.error && r2.error->column == 4 && r2.error->message.find("\xD1\x8F") != std::string::npos,
                "illegal UTF-8 character is reported as a whole");
    }
    {
        const LexResult r = lex("x = \"a\\qb\";");
        t.check(r.error && r.error->column == 7, "invalid escape error points at the backslash");
    }
    {
        const LexResult r = lex("a b a 1 1 \"s\" \"s\" b");
        t.check(r.ok() && r.identifiers.size() == 2 && r.constants.size() == 2 && r.tokens[2].index == 1 &&
                    r.tokens[7].index == 2 && r.tokens[6].index == 2,
                "repeated identifiers and constants reuse their index");
    }
    {
        const LexResult r = lex("x = 1;\ny = 2 & 3;");
        t.check(r.error && r.tokens.size() == 7 && r.error->line == 2, "tokens before the error are kept");
    }
}

}  // namespace

void runLexerTests(TestRunner& t) {
    testValidInput(t);
    testErrors(t);
    testValuesAndPositions(t);
}

}  // namespace minigroovy::test
