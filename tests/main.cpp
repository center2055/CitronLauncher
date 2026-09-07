#include "check.h"

#include <cstdio>

namespace tests {

namespace {
int g_failures = 0;
}

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void fail(const char* file, int line, const std::string& expr) {
    ++g_failures;
    std::printf("  FAILED %s(%d): %s\n", file, line, expr.c_str());
}

}

int main() {
    int run = 0;
    for (const auto& c : tests::registry()) {
        const int before = tests::g_failures;
        std::printf("%s\n", c.name);
        try {
            c.body();
        } catch (const std::exception& e) {
            tests::fail("exception", 0, e.what());
        }
        ++run;
        if (tests::g_failures != before) {
            std::printf("  -> failed\n");
        }
    }
    std::printf("%d tests, %d failures\n", run, tests::g_failures);
    return tests::g_failures == 0 ? 0 : 1;
}
