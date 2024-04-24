#include "vm.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>
#include <ostream>
#include <unordered_map>
#include <utility>

namespace script {
namespace {
enum class Kind { end, id, number, punctuation };
struct Token {
    Kind kind;
    std::string_view text;
    std::size_t line;
};
bool digit(char c) { return c >= '0' && c <= '9'; }
bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}
    Token next() {
        while (!source_.empty()) {
            const auto c = source_.front();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                if (c == '\n') ++line_;
                source_.remove_prefix(1);
            } else if (c == '#') {
                const auto end = source_.find('\n');
                source_.remove_prefix(std::min(end, source_.size()));
            } else break;
        }
        if (source_.empty()) return {Kind::end, {}, line_};
        const auto c = source_.front();
        std::size_t length = 1;
        auto kind = Kind::punctuation;
        if (letter(c) || digit(c)) {
            kind = letter(c) ? Kind::id : Kind::number;
            while (length < source_.size() && (digit(source_[length]) ||
                   (kind == Kind::id && letter(source_[length])))) ++length;
        } else if (source_.starts_with("==")) length = 2;
        auto text = source_.substr(0, length);
        source_.remove_prefix(length);
        return {kind, text, line_};
    }
private:
    std::string_view source_;
    std::size_t line_ = 1;
};

class Compiler {
    struct Call { std::size_t function, instruction, arity; std::string name; };
    struct Nesting {
        std::size_t& depth;
        explicit Nesting(std::size_t& count) : depth(count) {
            if (++depth > 128) throw Error("syntax nesting exceeds 128 levels");
        }
        ~Nesting() { --depth; }
    };
public:
    explicit Compiler(std::string_view source) : lexer_(source), token_(lexer_.next()) {}
    Program parse() {
        while (token_.kind != Kind::end) function();
        for (const auto& call : calls_) {
            const auto it = functions_.find(call.name);
            auto& inst = program_.functions[call.function].code[call.instruction];
            if (it == functions_.end() || program_.functions[it->second].arity != call.arity)
                throw Error("line " + std::to_string(inst.line) + ": invalid call to " + call.name);
            inst.arg = static_cast<Word>(it->second);
        }
        return std::move(program_);
    }
private:
    [[noreturn]] void error(std::string_view message) const {
        throw Error("line " + std::to_string(token_.line) + ": " + std::string(message));
    }
    Token take() { return std::exchange(token_, lexer_.next()); }
    bool match(std::string_view text) {
        if (token_.text != text) return false;
        take();
        return true;
    }
    void expect(std::string_view text) {
        if (!match(text)) error("expected " + std::string(text));
    }
    std::string identifier() {
        const auto text = token_.text;
        if (token_.kind != Kind::id || text == "fn" || text == "let" || text == "if" ||
            text == "else" || text == "while" || text == "return") error("expected identifier");
        return std::string(take().text);
    }
    Function& current() { return program_.functions.back(); }
    std::size_t emit(Op op, Word arg, std::size_t line) {
        auto& code = current().code;
        code.push_back({op, arg, line});
        return code.size() - 1;
    }
    Word address() { return static_cast<Word>(current().code.size()); }
    void patch(std::size_t jump) { current().code[jump].arg = address(); }
    Word declare(const std::string& name) {
        if (scopes_.back().contains(name)) error("duplicate local");
        auto slot = static_cast<Word>(current().locals++);
        scopes_.back().emplace(name, slot);
        return slot;
    }
    Word local(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
            if (auto slot = it->find(name); slot != it->end()) return slot->second;
        error("unknown local " + name);
    }
    void function() {
        expect("fn");
        auto name = identifier();
        if (!functions_.emplace(name, program_.functions.size()).second) error("duplicate function");
        program_.functions.push_back(Function{std::move(name)});
        scopes_.emplace_back();
        expect("(");
        if (!match(")")) {
            do { declare(identifier()); ++current().arity; } while (match(","));
            expect(")");
        }
        block();
        emit(Op::constant, 0, token_.line);
        emit(Op::ret, 0, token_.line);
        scopes_.pop_back();
    }
    void block() {
        Nesting guard(depth_);
        expect("{");
        scopes_.emplace_back();
        while (!match("}")) statement();
        scopes_.pop_back();
    }
    void statement() {
        const auto line = token_.line;
        if (match("let")) {
            const auto name = identifier();
            expect("=");
            expression();
            emit(Op::store, declare(name), line);
        } else if (match("return")) {
            expression();
            emit(Op::ret, 0, line);
        } else if (match("if")) {
            expression();
            const auto branch = emit(Op::branch, 0, line);
            block();
            if (match("else")) {
                const auto jump = emit(Op::jump, 0, line);
                patch(branch);
                block();
                patch(jump);
            } else patch(branch);
            return;
        } else if (match("while")) {
            const auto start = address();
            expression();
            const auto branch = emit(Op::branch, 0, line);
            block();
            emit(Op::jump, start, line);
            patch(branch);
            return;
        } else {
            const auto slot = local(identifier());
            expect("=");
            expression();
            emit(Op::store, slot, line);
        }
        expect(";");
    }
    static int precedence(std::string_view op) {
        if (op == "==") return 1;
        if (op == "<") return 2;
        if (op == "+" || op == "-") return 3;
        if (op == "*" || op == "/") return 4;
        return 0;
    }
    static Op binary(std::string_view op) {
        if (op == "==") return Op::eq;
        if (op == "<") return Op::lt;
        if (op == "+") return Op::add;
        if (op == "-") return Op::sub;
        if (op == "*") return Op::mul;
        return Op::div;
    }
    void expression(int minimum = 1) {
        Nesting guard(depth_);
        const auto line = token_.line;
        if (match("-")) {
            expression(5);
            emit(Op::neg, 0, line);
        } else if (match("(")) {
            expression();
            expect(")");
        } else if (token_.kind == Kind::number) {
            auto number = take().text;
            Word value{};
            if (std::from_chars(number.data(), number.data() + number.size(), value).ec != std::errc{})
                error("integer literal out of range");
            emit(Op::constant, value, line);
        } else {
            auto name = identifier();
            if (match("(")) {
                std::size_t args = 0;
                if (!match(")")) {
                    do { expression(); ++args; } while (match(","));
                    expect(")");
                }
                const auto index = emit(Op::call, 0, line);
                calls_.push_back({program_.functions.size() - 1, index, args, std::move(name)});
            } else emit(Op::load, local(name), line);
        }
        while (precedence(token_.text) >= minimum) {
            const auto op = take();
            expression(precedence(op.text) + 1);
            emit(binary(op.text), 0, op.line);
        }
    }
    Lexer lexer_;
    Token token_;
    Program program_;
    std::unordered_map<std::string, std::size_t> functions_;
    std::vector<std::unordered_map<std::string, Word>> scopes_;
    std::vector<Call> calls_;
    std::size_t depth_{};
};

constexpr std::string_view name(Op op) {
    constexpr std::string_view names[] = {"const", "load", "store", "add", "sub", "mul", "div",
        "neg", "lt", "eq", "jump", "branch_if_zero", "call", "return"};
    return names[static_cast<std::size_t>(op)];
}
void print(const Instruction& inst, std::ostream& out) {
    out << name(inst.op);
    if (inst.op == Op::constant || inst.op == Op::load || inst.op == Op::store ||
        inst.op == Op::jump || inst.op == Op::branch || inst.op == Op::call)
        out << ' ' << inst.arg;
    out << "  [line " << inst.line << "]\n";
}
}

Program compile(std::string_view source) {
    if (source.size() > 1024 * 1024) throw Error("source exceeds 1 MiB");
    return Compiler(source).parse();
}
void disassemble(const Program& program, std::ostream& out) {
    for (std::size_t fn = 0; fn < program.functions.size(); ++fn) {
        const auto& function = program.functions[fn];
        out << "fn " << fn << ' ' << function.name << '/' << function.arity << '\n';
        for (std::size_t ip = 0; ip < function.code.size(); ++ip) {
            out << "  " << ip << "  ";
            print(function.code[ip], out);
        }
    }
}

Machine::Machine(Program program, std::size_t fuel) : program_(std::move(program)), fuel_(fuel) {
    const auto& list = program_.functions;
    auto it = std::find_if(list.begin(), list.end(), [](const auto& fn) { return fn.name == "main"; });
    if (it == list.end() || it->arity) throw Error("expected fn main() as the entry point");
    frames_.push_back({static_cast<std::size_t>(it - list.begin()), 0, 0, std::vector<Word>(it->locals)});
}
Word Machine::pop() {
    if (stack_.size() <= frames_.back().base) throw Error("empty operand stack");
    auto value = stack_.back();
    stack_.pop_back();
    return value;
}
void Machine::step() {
    if (done()) return;
    if (!fuel_) throw Error("instruction budget exhausted");
    --fuel_;
    auto& frame = frames_.back();
    const auto& code = program_.functions.at(frame.function).code;
    const auto inst = code.at(frame.ip++);
    const auto arg = static_cast<std::size_t>(inst.arg);
    switch (inst.op) {
    case Op::constant: stack_.push_back(inst.arg); return;
    case Op::load: stack_.push_back(frame.locals.at(arg)); return;
    case Op::store: frame.locals.at(arg) = pop(); return;
    case Op::jump: frame.ip = arg; return;
    case Op::branch: if (!pop()) frame.ip = arg; return;
    case Op::neg: stack_.push_back(std::bit_cast<Word>(std::uint64_t{0} - std::uint64_t(pop()))); return;
    case Op::call: {
        const auto& fn = program_.functions.at(arg);
        if (frames_.size() >= 256 || stack_.size() - frame.base < fn.arity)
            throw Error("call exceeds depth or operand limits");
        const auto base = stack_.size() - fn.arity;
        std::vector<Word> locals(fn.locals);
        std::copy(stack_.begin() + base, stack_.end(), locals.begin());
        stack_.resize(base);
        frames_.push_back({arg, 0, base, std::move(locals)});
        return;
    }
    case Op::ret: {
        const auto value = pop();
        stack_.resize(frame.base);
        frames_.pop_back();
        if (done()) result_ = value; else stack_.push_back(value);
        return;
    }
    default: break;
    }
    const auto b = pop(), a = pop();
    Word result{};
    switch (inst.op) {
    case Op::add: result = std::bit_cast<Word>(std::uint64_t(a) + std::uint64_t(b)); break;
    case Op::sub: result = std::bit_cast<Word>(std::uint64_t(a) - std::uint64_t(b)); break;
    case Op::mul: result = std::bit_cast<Word>(std::uint64_t(a) * std::uint64_t(b)); break;
    case Op::div:
        if (!b || (a == std::numeric_limits<Word>::min() && b == -1))
            throw Error("integer division out of range");
        result = a / b;
        break;
    case Op::lt: result = a < b; break;
    case Op::eq: result = a == b; break;
    default: throw Error("invalid opcode");
    }
    stack_.push_back(result);
}
Word Machine::run() { while (!done()) step(); return result_; }
Word Machine::result() const {
    if (!done()) throw Error("execution still in progress");
    return result_;
}
}
