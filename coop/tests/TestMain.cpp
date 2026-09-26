#include "TestMain.h"

#include <cstdio>
#include <cstring>
#include <exception>

void coop_test::Fail(const char* file, int line, const std::string& what) {
    std::printf("    FAIL %s:%d: %s\n", file, line, what.c_str());
    throw Failure{};
}

// Usage: coop-tests [substring]  -> runs every test whose name contains the substring.
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failed = 0;
    for (const auto& testCase : coop_test::Cases()) {
        if (filter != nullptr && std::strstr(testCase.name, filter) == nullptr) {
            continue;
        }
        run++;
        std::printf("[ RUN  ] %s\n", testCase.name);
        try {
            testCase.fn();
            std::printf("[  OK  ] %s\n", testCase.name);
        } catch (const coop_test::Failure&) {
            failed++;
            std::printf("[FAILED] %s\n", testCase.name);
        } catch (const std::exception& e) {
            failed++;
            std::printf("[FAILED] %s (exception: %s)\n", testCase.name, e.what());
        }
    }
    std::printf("\n%d/%d tests passed\n", run - failed, run);
    return failed == 0 ? 0 : 1;
}
