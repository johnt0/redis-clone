#include "tester.hpp"
#include <iostream>

int main() {
    std::cout << "Running " << Tester::instance().tests.size() << " tests...\n";
    int failures = Tester::instance().run();
    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    } else {
        std::cerr << failures << " tests failed.\n";
        return 1;
    }
}
