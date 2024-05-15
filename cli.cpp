#include "vm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) try {
    if (argc != 3) throw script::Error("usage: game_vm run|disasm|debug FILE");
    const std::string_view command(argv[1]);
    std::ifstream file(argv[2], std::ios::binary);
    if (!file || std::filesystem::file_size(argv[2]) > 1024 * 1024)
        throw script::Error("cannot read script or script exceeds 1 MiB");
    const std::string source((std::istreambuf_iterator<char>(file)), {});
    auto program = script::compile(source);
    if (command == "disasm") {
        script::disassemble(program, std::cout);
        return 0;
    }
    script::Machine vm(std::move(program));
    if (command == "run") {
        const auto value = vm.run();
        std::cout << "result: " << value << '\n';
    } else if (command == "debug") {
        std::cout << "s step, p inspect, c continue, q quit\n";
        vm.inspect(std::cout);
        for (std::string line; !vm.done() && std::cout << "> " && std::getline(std::cin, line);) {
            if (line == "q") break;
            if (line == "s") vm.step();
            else if (line == "c") vm.run();
            else if (line != "p") { std::cout << "Use s, p, c or q.\n"; continue; }
            vm.inspect(std::cout);
        }
    } else throw script::Error("unknown command");
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
