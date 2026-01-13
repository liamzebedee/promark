#include "test_helpers.h"
#include <iostream>
#include <cstring>

// Include all test files to register them
// (They use REGISTER_TEST macro which registers at static init time)

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string specificTest;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            specificTest = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --test <name>  Run only the specified test\n"
                      << "  --help, -h     Show this help message\n"
                      << "\n"
                      << "Environment variables:\n"
                      << "  TEST_OUTPUT_DIR  Directory for screenshot output (default: /tmp/promark_tests)\n";
            return 0;
        }
    }

    if (!specificTest.empty()) {
        return TestRunner::instance().runTest(specificTest);
    }

    return TestRunner::instance().runAll();
}
