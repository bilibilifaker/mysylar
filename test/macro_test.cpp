#include "../src/macro.h"
#include <assert.h>
#include "../src/util.h"
#include "../src/log.h"

sylar::Logger::ptr logger = SYLAR_LOG_ROOT();

void test_assert() {
    SYLAR_LOG_INFO(logger) << std::endl << sylar::BacktraceToString(10);
    // SYLAR_ASSERT(false);
    SYLAR_ASSERT2(0 ==  1, "ABCDEFG");
}

int main() {
    test_assert();
    return 0;
}