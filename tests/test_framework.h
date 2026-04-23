#pragma once

#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace game2048::test {

using TestFn = std::function<void()>;

struct TestCase {
    std::string name;
    TestFn fn;
};

std::vector<TestCase>& Registry();

struct Registrar {
    Registrar(std::string name, TestFn fn);
};

class AssertionFailure : public std::runtime_error {
public:
    explicit AssertionFailure(const std::string& message)
        : std::runtime_error(message) {}
};

[[noreturn]] inline void Fail(const std::string& message) {
    throw AssertionFailure(message);
}

template <typename T, typename U>
void ExpectEqual(const T& actual, const U& expected, const char* exprActual, const char* exprExpected,
                 const char* file, int line) {
    if (!(actual == expected)) {
        std::ostringstream oss;
        oss << file << ':' << line << " expected " << exprActual << " == " << exprExpected
            << " but values differed";
        Fail(oss.str());
    }
}

inline void ExpectTrue(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        std::ostringstream oss;
        oss << file << ':' << line << " expected true: " << expr;
        Fail(oss.str());
    }
}

inline void ExpectFalse(bool condition, const char* expr, const char* file, int line) {
    if (condition) {
        std::ostringstream oss;
        oss << file << ':' << line << " expected false: " << expr;
        Fail(oss.str());
    }
}

inline void ExpectNear(double actual, double expected, double tolerance, const char* exprActual,
                       const char* exprExpected, const char* file, int line) {
    if (std::fabs(actual - expected) > tolerance) {
        std::ostringstream oss;
        oss << file << ':' << line << " expected " << exprActual << " ~= " << exprExpected
            << " with tolerance " << tolerance << " but got [" << actual << "] vs [" << expected << ']';
        Fail(oss.str());
    }
}

}  // namespace game2048::test

#define TEST_CASE(name)                                                                                   \
    void name();                                                                                          \
    static ::game2048::test::Registrar name##_registrar(#name, &name);                                   \
    void name()

#define EXPECT_EQ(actual, expected)                                                                       \
    ::game2048::test::ExpectEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)
#define EXPECT_TRUE(expr) ::game2048::test::ExpectTrue((expr), #expr, __FILE__, __LINE__)
#define EXPECT_FALSE(expr) ::game2048::test::ExpectFalse((expr), #expr, __FILE__, __LINE__)
#define EXPECT_NEAR(actual, expected, tolerance)                                                          \
    ::game2048::test::ExpectNear((actual), (expected), (tolerance), #actual, #expected, __FILE__, __LINE__)
