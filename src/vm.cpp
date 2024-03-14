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

}
}
