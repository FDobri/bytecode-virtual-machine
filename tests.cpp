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
TEST(Vm, BoundsRunawayExecution) {
    script::Machine vm(script::compile("fn main(){ while 1 { } }"), 50);
    EXPECT_THROW(vm.run(), script::Error);
}
TEST(Vm, DivisionAndCallErrors) {
    EXPECT_THROW((void)script::compile("fn main(){ return missing(1); }"), script::Error);
    script::Machine vm(script::compile("fn main(){ return 7/0; }"));
    EXPECT_THROW(vm.run(), script::Error);
}
