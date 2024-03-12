#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace script {
using Word = std::int64_t;
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
enum class Op { constant, load, store, add, sub, mul, div, neg, lt, eq, jump, branch, call, ret };
struct Instruction { Op op; Word arg; std::size_t line; };
struct Function {
    std::string name;
    std::size_t arity{}, locals{};
    std::vector<Instruction> code;
};
struct Program { std::vector<Function> functions; };
Program compile(std::string_view source);
void disassemble(const Program& program, std::ostream& output);

}
