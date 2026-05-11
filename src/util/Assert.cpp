#include "util/Assert.h"

#include <cstdlib>
#include <cstdio>

#ifndef NDEBUG
namespace xoj::util {
void assertFailure(const char* expr, const std::string& msg, const char* fileName, int line, const char* funcName) {
    std::fprintf(stderr, "Assertion failed: %s\n%s    in function %s\n    at line %d of %s\n", expr,
                 (!msg.empty() ? std::string("    Message: ") + msg + "\n" : "").c_str(), funcName, line, fileName);
    std::abort();
}
};  // namespace vn::util
#endif
