# NOMAIN Programming Language Implementation

## Overview
This is the implementation of NOMAIN v0.1 - a programming language with centralized data storage and flexible entry points. NOMAIN programs use the `.nom` file extension.

## Project Structure
```
nomain/
├── src/
│   ├── token_types.py    # Token type definitions
│   ├── lexer.py          # Lexer/Tokenizer
│   ├── parser.py         # Parser (TODO)
│   ├── ast_nodes.py      # AST node definitions (TODO)
│   ├── semantic.py       # Type checking (TODO)
│   └── interpreter.py    # Interpreter (TODO)
├── tests/
│   ├── test_lexer.py     # Lexer tests
│   ├── test_parser.py    # Parser tests
│   └── programs/         # Test NOMAIN programs
│       ├── hello.nom
│       └── math.nom
├── examples/             # Example programs (TODO)
└── README.md
```

## Current Status

### ✅ Completed
- Token type definitions
- Complete lexer/tokenizer
- Lexer test suite
- AST node definitions
- Parser implementation
- Interpreter
- Built-in I/O functions (say, say_f, INPUT, INPUT_INT, INPUT_FLOAT)
- Quantum computing functions (CreateQubit, ApplyGate, Measure, etc.)
- Example NOMAIN programs (10+ working examples)

### 📋 Optional Future Enhancements
- Semantic analyzer (type checking before execution)
- Compiler (to native code or bytecode)
- Standard library expansion
- IDE with syntax highlighting and error detection

## Quick Start

### Run a NOMAIN program:
```bash
make exec-hello        # Run hello.nom
make exec-math         # Run math.nom
make exec-quantum_demo # Run quantum demo
```

### Parse and view AST:
```bash
make parse-hello       # Show compact AST
make ast-hello         # Show detailed AST with expressions
```

### Run all tests:
```bash
make test              # Run lexer and parser tests
make test-lexer        # Run only lexer tests
make test-parser       # Run only parser tests
make run-examples      # Execute all example programs
```

### List available programs:
```bash
make list-programs     # Show all .nom files
```

## Language Features (v0.1)

- Single `[DATA_STORAGE]` section
- `[GLOBAL]` data scope
- Basic types: INT, FLOAT, STRING, BOOL
- Functions with parameters and return values
- `[APPLICATION-STARTUP]` tag for entry point
- Control flow: IF/THEN/ELSE, WHILE, FOR
- Basic operators: +, -, *, /, ==, !=, <, >, <=, >=
- Logical operators: AND, OR, NOT
- Comments: // and /* */

## Writing NOMAIN Programs

NOMAIN programs (`.nom` files) have a specific structure:

```nomain
[DATA_STORAGE]
[GLOBAL]
    INT x = 10;
    STRING message = "Hello";

[APPLICATION-STARTUP]
function main() -> [VOID] {
    say(message);
    INT result = add(x, 5);
    say_f("Result: ", result);
}

function add(INT a, INT b) -> INT {
    RETURN a + b;
}
```

### Key Features:
- **Centralized Data Storage**: All global variables in `[DATA_STORAGE]` section
- **Type System**: INT, FLOAT, STRING, BOOL, QUBIT, CIRCUIT, GATE, QSTATE
- **Functions**: Support parameters and return values
- **Control Flow**: IF/THEN/ELSE, WHILE, FOR loops
- **Built-in Functions**: I/O (say, INPUT) and quantum operations
- **Quantum Computing**: Native support for qubits and quantum gates

## Documentation

See the full language specifications:
- `/home/dmort/NOMAIN_LANGUAGE_SPEC.md` - Complete specification
- `/home/dmort/NOMAIN_MINIMAL_v0.1_SPEC.md` - v0.1 minimal spec
