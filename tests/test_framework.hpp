#pragma once

// Мінімальний каркас для тестів: без сторонніх бібліотек, щоб збірка
// лишалася однією командою g++.

#include <iostream>
#include <string>

namespace minigroovy::test {

class TestRunner {
public:
    // Записує результат однієї перевірки; при невдачі друкує назву й деталі.
    void check(bool condition, const std::string& name, const std::string& detail = "") {
        ++total_;
        if (condition) return;
        ++failed_;
        std::cout << "[FAIL] " << name;
        if (!detail.empty()) std::cout << "\n       " << detail;
        std::cout << '\n';
    }

    int total() const { return total_; }
    int failed() const { return failed_; }

private:
    int total_ = 0;
    int failed_ = 0;
};

}  // namespace minigroovy::test
