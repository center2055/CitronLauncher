#pragma once

#include <functional>
#include <string>
#include <vector>

namespace tests {

struct Case {
    const char* name;
    std::function<void()> body;
};

std::vector<Case>& registry();
void fail(const char* file, int line, const std::string& expr);

struct Register {
    Register(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

}

#define TEST_CASE(name) \
    static void name##_body(); \
    static tests::Register name##_reg(#name, name##_body); \
    static void name##_body()

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            tests::fail(__FILE__, __LINE__, #expr); \
        } \
    } while (false)

#define CHECK_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            tests::fail(__FILE__, __LINE__, std::string(#a " == " #b)); \
        } \
    } while (false)
