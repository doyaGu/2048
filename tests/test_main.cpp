#include <iostream>
#include "test_framework.h"

namespace game2048::test {

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

Registrar::Registrar(std::string name, TestFn fn) {
    Registry().push_back({std::move(name), std::move(fn)});
}

}  // namespace game2048::test

int main() {
    int failures = 0;
    for (const auto& test : game2048::test::Registry()) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failures;
            std::cout << "[FAIL] " << test.name << '\n' << "  " << ex.what() << '\n';
        }
    }

    std::cout << "Ran " << game2048::test::Registry().size() << " tests, failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
