# Semantic Error Test Suite

This directory contains test programs that are **intentionally invalid** to demonstrate various semantic errors in NOMAIN.

## Error Categories

### Type Errors
- `error_type_mismatch.nom` - Assigning wrong type to variable
- `error_wrong_arg_types.nom` - Passing wrong types to function

### Function Call Errors
- `error_too_few_args.nom` - Missing function arguments
- `error_too_many_args.nom` - Extra function arguments
- `error_undefined_function.nom` - Calling non-existent function

### Declaration Errors
- `error_undefined.nom` - Using undeclared variable
- `error_use_before_declaration.nom` - Using variable before it's declared
- `error_duplicate_variable.nom` - Declaring same variable twice
- `error_duplicate_function.nom` - Defining same function twice

### Runtime Errors
- `error_const_modification.nom` - Trying to modify CONST variable
- `error_division_by_zero.nom` - Division by zero

## Usage

These tests are used for:

1. **Documentation** - Show users what errors look like
2. **IDE Development** - Test error detection and reporting
3. **Semantic Analyzer** - Test cases for type checker (future)
4. **Compiler Development** - Ensure proper error messages

## Running Error Tests

Each file should produce an error when executed:

```bash
# Should fail with specific error message
python3 src/interpreter.py tests/semantic_errors/error_undefined.nom

# Test that parser still works (some errors are runtime-only)
make parse-<filename>
```

## Expected Behavior

Most of these errors are caught at **runtime** by the interpreter. A future semantic analyzer could catch many of these at **compile time** before execution.

### ✅ Currently Caught at Runtime:
- Undefined variables/functions
- Wrong argument counts (too few/too many)
- CONST modifications
- Division by zero
- Wrong argument types (when operations fail)
- Use before declaration

### ⚠️ NOT Currently Caught (would require semantic analyzer):
- **Type mismatches in assignments** - `INT x = "string"` is accepted by parser
- **Duplicate variable declarations** - Last declaration wins
- **Duplicate function definitions** - Last definition wins

### 📋 Could be Added with Semantic Analyzer:
- Static type checking before execution
- Dead code detection
- Unused variable warnings
- Return type verification
- Exhaustive case checking
