import re
from pathlib import Path

# Basic regex to match function definitions and calls
FUNC_DEF = re.compile(r'\b(\w+)\s+\**(\w+)\s*\([^)]*\)\s*\{')
FUNC_CALL = re.compile(r'\b(\w+)\s*\([^;]*\);')

def parse_file(path):
    with open(path, 'r') as f:
        lines = f.readlines()

    functions = {}
    current_func = None
    for line in lines:
        def_match = FUNC_DEF.match(line.strip())
        if def_match:
            current_func = def_match.group(2)
            functions[current_func] = []
        elif current_func:
            calls = FUNC_CALL.findall(line)
            functions[current_func].extend(calls)

    return functions

def build_call_tree(functions, root, depth=0, visited=None):
    if visited is None:
        visited = set()
    visited.add(root)
    indent = '  ' * depth
    print(f"{indent}{root}")
    for callee in functions.get(root, []):
        if callee not in visited:
            build_call_tree(functions, callee, depth + 1, visited)

# Usage
functions = parse_file("kernel.c")
build_call_tree(functions, "kernel_main")