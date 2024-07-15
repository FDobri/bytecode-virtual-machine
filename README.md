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

The handwritten lexer and precedence parser compile directly to stack bytecode, patch control flow and resolve function calls after parsing. Add, subtract, multiply and negate wrap modulo 2⁶⁴. Division is signed and reports divide-by-zero and signed overflow. A zero value is false. Call depth, syntax nesting and execution fuel have fixed limits. The CLI starts at `fn main()`.
