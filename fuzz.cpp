#include "vm.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > 16384) return 0;
    try {
        auto code = script::compile(std::string_view(reinterpret_cast<const char*>(data), size));
        script::Machine vm(std::move(code), 2048);
        vm.run();
    } catch (const script::Error&) {}
    return 0;
}
