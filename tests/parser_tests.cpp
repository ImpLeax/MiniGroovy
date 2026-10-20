// Модульні тести синтаксичного аналізатора на коротких фрагментах коду.

#include "parser_tests.hpp"

#include <format>
#include <string>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace minigroovy::test {

namespace {

// Лексичний і синтаксичний аналіз фрагмента. Лексична помилка в тестах парсера
// не очікується, тому повідомляється як окрема невдача.
struct Outcome {
    bool lexOk = false;
    ParseResult parse;
};

Outcome analyze(const std::string& source) {
    const LexResult lexed = Lexer(source).run();
    if (!lexed.ok()) return {};
    return {true, Parser(lexed.tokens).run()};
}

std::string describe(const Outcome& outcome) {
    if (!outcome.lexOk) return "lexical error";
    if (outcome.parse.ok()) return "parsed successfully";
    const SyntaxError& e = *outcome.parse.error;
    return std::format("error at {}:{}: {}", e.line, e.column, e.message);
}

// Фрагмент має бути синтаксично коректним.
void expectValid(TestRunner& t, const std::string& name, const std::string& source) {
    const Outcome outcome = analyze(source);
    t.check(outcome.lexOk && outcome.parse.ok(), name, describe(outcome));
}

// Розбір має аварійно завершитися в рядку line з повідомленням, що містить fragment.
void expectSyntaxError(TestRunner& t, const std::string& name, const std::string& source, int line,
                       const std::string& fragment) {
    const Outcome outcome = analyze(source);
    const auto& error = outcome.parse.error;
    const bool ok = outcome.lexOk && error && error->line == line && error->message.find(fragment) != std::string::npos;
    t.check(ok, name, std::format("expected error at line {} with \"{}\", got {}", line, fragment, describe(outcome)));
}

void testDeclarations(TestRunner& t) {
    expectValid(t, "empty program", "");
    expectValid(t, "only comments", "// nothing here\n");
    expectValid(t, "variable declarations", "int a; float b = 1.5; boolean c = true; String s = \"x\";");
    expectValid(t, "constant declaration", "final int N = 10;");
    expectValid(t, "array declarations", "int a[3]; int b[N] = {1, 2, 3}; final int M = 2;");
    expectValid(t, "enum declaration", "enum Color { RED, GREEN, BLUE }");
    expectValid(t, "range type declarations", "type P = 0..100; type D = -5..5; type L = LOW..HIGH;");
    expectValid(t, "user type in declaration", "Color c = Color.RED; P p;");
    expectValid(t, "function declarations",
                "int f(int a, float b[], Color c) { return a; } void g() { return; } P h() { return 1; }");
}

void testStatements(TestRunner& t) {
    expectValid(t, "assignments", "x = 1; a[i + 1] = a[i] * 2;");
    expectValid(t, "output and input", "print(\"x\"); s = read();");
    expectValid(t, "call statements", "f(); g(1, x + 2, h(3));");
    expectValid(t, "if without else", "if (a) { x = 1; }");
    expectValid(t, "if with else", "if (a) { x = 1; } else { x = 2; }");
    expectValid(t, "while loop", "while (i < 3) { i = i + 1; }");
    expectValid(t, "for loop with declaration", "for (int i = 0; i < 3; i = i + 1) { print(i); }");
    expectValid(t, "for loop with empty parts", "for (;;) { break; }");
    expectValid(t, "for loop with assignment and call", "for (i = 0; i < n; step(i)) { }");
    expectValid(t, "switch with fallthrough and default",
                "switch (x) { case 0: case 1: print(1); break; case -2: case \"s\": case Color.RED: case N: "
                "default: print(0); }");
    expectValid(t, "empty switch", "switch (x) { }");
    expectValid(t, "nested blocks", "{ { int a = 1; { a = 2; } } }");
    expectValid(t, "local declarations in blocks", "void f() { final int K = 1; int a[K]; State s; }");
    expectValid(t, "return and break are accepted anywhere by the grammar", "return; break;");
}

void testExpressions(TestRunner& t) {
    expectValid(t, "arithmetic priority", "x = a + b * c - d / e;");
    expectValid(t, "power and unary minus", "x = -2 ** -3 ** 2;");
    expectValid(t, "logical operators", "b = !a && (c || d) && x != y;");
    expectValid(t, "relational inside equality", "b = a < b == c >= d;");
    expectValid(t, "conversions", "x = int(\"5\") + int(float(y) * 2.0);");
    expectValid(t, "names: element, enum value, call", "x = a[i] + Color.RED + f(1, 2) + g();");
    expectValid(t, "parenthesised expression", "x = ((a + b) * (c - d));");
    expectValid(t, "left associative chains", "x = a - b - c; y = a / b / c;");
}

void testErrors(TestRunner& t) {
    // Пропуск термінала
    expectSyntaxError(t, "missing ';'", "int a = 1\nint b = 2;", 2, "expected ';' but found 'int'");
    expectSyntaxError(t, "missing ')' in if", "if (a {\n}", 1, "expected ')' but found '{'");
    expectSyntaxError(t, "missing '}' at end", "while (a) {\nx = 1;", 2, "unexpected end of program, expected '}'");
    expectSyntaxError(t, "missing '=' in constant", "final int N 5;", 1, "expected '=' but found '5'");
    expectSyntaxError(t, "missing ':' after case", "switch (x) {\ncase 1 print(x);\n}", 2,
                      "expected ':' but found 'print'");
    expectSyntaxError(t, "missing ';' in for header", "for (int i = 0, i < 3; ) { }", 1,
                      "expected ';' but found ','");

    // Зайвий термінал
    expectSyntaxError(t, "extra 'if'", "if if (a) { }", 1, "expected '(' but found 'if'");
    expectSyntaxError(t, "extra ')'", "print(x));", 1, "expected ';' but found ')'");
    expectSyntaxError(t, "extra '='", "x = = 5;", 1, "expected expression but found '='");
    expectSyntaxError(t, "extra ';' after enum is a statement error", "enum E { A };", 1,
                      "expected statement but found ';'");

    // Помилка в наборі ключового слова
    expectSyntaxError(t, "misspelled while", "whlie (i < 3) {\ni = i + 1;\n}", 1, "expected ';' but found '{'");
    expectSyntaxError(t, "misspelled else", "if (a) { }\nesle { }", 2, "expected '=' but found '{'");
    expectSyntaxError(t, "misspelled print with string", "pritn \"x\";", 1, "expected '=' but found '\"x\"'");

    // Помилковий оператор у виразі
    expectSyntaxError(t, "two binary operators in a row", "x = a + * b;", 1, "expected expression but found '*'");
    expectSyntaxError(t, "unary plus is not an operator", "x = +a;", 1, "expected expression but found '+'");
    expectSyntaxError(t, "range operator in expression", "int t = 5 .. 7;", 1, "expected ';' but found '..'");
    expectSyntaxError(t, "assignment inside condition", "if (a = 1) { }", 1, "expected ')' but found '='");
    expectSyntaxError(t, "relational operators are non-associative", "b = a < b < c;", 1, "non-associative");
    expectSyntaxError(t, "equality operators are non-associative", "b = a == b != c;", 1, "non-associative");

    // Оголошення не на своєму рівні та інші конструкції
    expectSyntaxError(t, "function inside a block", "void f() {\nint g() { }\n}", 2, "only at the global level");
    expectSyntaxError(t, "enum inside a function", "void f() {\nenum E { A }\n}", 2, "only at the global level");
    expectSyntaxError(t, "statement before the first case", "switch (x) {\nprint(x);\n}", 2,
                      "expected 'case', 'default' or '}'");
    expectSyntaxError(t, "else if is not in the grammar", "if (a) { } else if (b) { }", 1,
                      "expected '{' but found 'if'");
    expectSyntaxError(t, "read is not a statement", "read();", 1, "expected statement but found 'read'");
    expectSyntaxError(t, "unexpected end inside an expression", "x = 1 +", 1, "unexpected end of program");
    expectSyntaxError(t, "empty argument", "f(1, );", 1, "expected expression but found ')'");
}

void testPositions(TestRunner& t) {
    {
        const Outcome o = analyze("int a = 1;\n  x = ;");
        t.check(o.lexOk && o.parse.error && o.parse.error->line == 2 && o.parse.error->column == 7,
                "error position points at the unexpected token", describe(o));
    }
    {
        const Outcome o = analyze("x = (1 + 2");
        t.check(o.lexOk && o.parse.error && o.parse.error->line == 1 && o.parse.error->column == 11,
                "unexpected end points right after the last token", describe(o));
    }
}

}  // namespace

void runParserTests(TestRunner& t) {
    testDeclarations(t);
    testStatements(t);
    testExpressions(t);
    testErrors(t);
    testPositions(t);
}

}  // namespace minigroovy::test
