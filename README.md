# Bytecode Interpreter and Virtual Machine

A small signed 64-bit integer scripting language for gameplay rules. It supports local variables, lexical blocks, arithmetic, comparison, if/else, while loops and functions with positional parameters.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./build/game_vm run examples/wave.gs
./build/game_vm disasm examples/wave.gs
./build/game_vm debug examples/damage.gs
```

On multi-config builds use `build/Release/`. The debugger's `s` command executes one opcode. Use `p` to inspect the current instruction, locals and operand stack or `c` to run to completion.
