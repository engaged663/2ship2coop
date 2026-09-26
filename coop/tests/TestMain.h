#pragma once
// Minimal test harness (no third-party deps). A failing CHECK aborts the current test case.
#include <string>
#include <vector>

namespace coop_test {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& Cases() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) {
        Cases().push_back({ name, fn });
    }
};

struct Failure {};

void Fail(const char* file, int line, const std::string& what);

} // namespace coop_test

#define TEST_CASE(name)                                                  \
    static void name();                                                  \
    static coop_test::Registrar name##_registrar(#name, name);           \
    static void name()

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            coop_test::Fail(__FILE__, __LINE__, #cond);                  \
        }                                                                \
    } while (0)

#define CHECK_EQ(a, b)                                                   \
    do {                                                                 \
        if (!((a) == (b))) {                                             \
            coop_test::Fail(__FILE__, __LINE__, #a " == " #b);           \
        }                                                                \
    } while (0)
