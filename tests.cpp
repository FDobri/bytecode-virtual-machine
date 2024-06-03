#include "vm.hpp"
#include <gtest/gtest.h>
#include <sstream>

TEST(Vm, RunsFunctionsWithLexicalScopes) {
    const char* source = "fn main(){ let n=5; if n==5 { let n=2; } return product(n); } "
                         "fn product(n){ if n<2 { return 1; } return n * product(n-1); }";
    script::Machine vm(script::compile(source));
    vm.step();
    std::ostringstream state;
    vm.inspect(state);
    EXPECT_NE(state.str().find("stack: 5"), std::string::npos);
    EXPECT_EQ(vm.run(), 120);
}
