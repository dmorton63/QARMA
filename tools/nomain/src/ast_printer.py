"""
Detailed AST printer for debugging
Shows full expression details instead of <expr> placeholders
"""

from ast_nodes import *


def print_detailed_ast(node, indent=0):
    """Print full AST with all expression details"""
    prefix = "  " * indent
    
    if isinstance(node, Program):
        print(f"{prefix}Program:")
        print(f"{prefix}  DataStorage:")
        print_detailed_ast(node.data_storage, indent + 2)
        print(f"{prefix}  Functions ({len(node.functions)}):")
        for func in node.functions:
            print_detailed_ast(func, indent + 2)
        print(f"{prefix}  Startup: {node.startup_function}")
    
    elif isinstance(node, DataStorage):
        if node.declarations:
            for decl in node.declarations:
                print_detailed_ast(decl, indent)
        else:
            print(f"{prefix}(empty)")
    
    elif isinstance(node, GlobalDeclaration):
        const_str = "CONST " if node.is_const else ""
        init_str = ""
        if node.initial_value:
            init_str = f" = {format_expression(node.initial_value)}"
        print(f"{prefix}{const_str}{node.var_type} {node.name}{init_str}")
    
    elif isinstance(node, FunctionDef):
        startup_str = "[APPLICATION-STARTUP] " if node.is_startup else ""
        params_str = ", ".join(f"{p.var_type} {p.name}" for p in node.parameters)
        print(f"{prefix}{startup_str}function {node.name}({params_str}) -> {node.return_type}:")
        for stmt in node.body:
            print_detailed_ast(stmt, indent + 1)
    
    elif isinstance(node, AssignmentStmt):
        expr_str = format_expression(node.value)
        print(f"{prefix}{node.variable} = {expr_str}")
    
    elif isinstance(node, ReturnStmt):
        if node.value:
            expr_str = format_expression(node.value)
            print(f"{prefix}RETURN {expr_str}")
        else:
            print(f"{prefix}RETURN")
    
    elif isinstance(node, IfStmt):
        cond_str = format_expression(node.condition)
        print(f"{prefix}IF {cond_str} THEN")
        for stmt in node.then_block:
            print_detailed_ast(stmt, indent + 1)
        if node.else_block:
            print(f"{prefix}ELSE:")
            for stmt in node.else_block:
                print_detailed_ast(stmt, indent + 1)
    
    elif isinstance(node, WhileStmt):
        cond_str = format_expression(node.condition)
        print(f"{prefix}WHILE {cond_str}:")
        for stmt in node.body:
            print_detailed_ast(stmt, indent + 1)
    
    elif isinstance(node, ForStmt):
        init_str = format_statement(node.init)
        cond_str = format_expression(node.condition)
        inc_str = format_statement(node.increment)
        print(f"{prefix}FOR {init_str}; {cond_str}; {inc_str}:")
        for stmt in node.body:
            print_detailed_ast(stmt, indent + 1)
    
    elif isinstance(node, VarDeclStmt):
        init_str = ""
        if node.initial_value:
            init_str = f" = {format_expression(node.initial_value)}"
        print(f"{prefix}{node.var_type} {node.name}{init_str}")
    
    elif isinstance(node, ExpressionStmt):
        expr_str = format_expression(node.expression)
        print(f"{prefix}{expr_str};")
    
    else:
        print(f"{prefix}{node.__class__.__name__}")


def format_expression(expr):
    """Format an expression as a readable string"""
    if isinstance(expr, Literal):
        if isinstance(expr.value, str):
            return f'"{expr.value}"'
        return str(expr.value)
    
    elif isinstance(expr, Variable):
        return expr.name
    
    elif isinstance(expr, BinaryOp):
        left = format_expression(expr.left)
        right = format_expression(expr.right)
        op_str = get_operator_string(expr.operator)
        return f"({left} {op_str} {right})"
    
    elif isinstance(expr, UnaryOp):
        operand = format_expression(expr.operand)
        op_str = get_operator_string(expr.operator)
        return f"{op_str}{operand}"
    
    elif isinstance(expr, FunctionCall):
        args_str = ", ".join(format_expression(arg) for arg in expr.arguments)
        return f"{expr.name}({args_str})"
    
    else:
        return f"<{expr.__class__.__name__}>"


def format_statement(stmt):
    """Format a statement as a readable string (for single-line contexts)"""
    if isinstance(stmt, VarDeclStmt):
        init_str = ""
        if stmt.initial_value:
            init_str = f" = {format_expression(stmt.initial_value)}"
        return f"{stmt.var_type} {stmt.name}{init_str}"
    
    elif isinstance(stmt, AssignmentStmt):
        expr_str = format_expression(stmt.value)
        return f"{stmt.variable} = {expr_str}"
    
    else:
        return f"<{stmt.__class__.__name__}>"


def get_operator_string(token_type):
    """Convert token type to operator string"""
    from token_types import TokenType
    
    op_map = {
        TokenType.PLUS: '+',
        TokenType.MINUS: '-',
        TokenType.MULTIPLY: '*',
        TokenType.DIVIDE: '/',
        TokenType.EQ: '==',
        TokenType.NE: '!=',
        TokenType.LT: '<',
        TokenType.GT: '>',
        TokenType.LE: '<=',
        TokenType.GE: '>=',
        TokenType.AND: 'AND',
        TokenType.OR: 'OR',
        TokenType.NOT: 'NOT ',
    }
    
    return op_map.get(token_type, str(token_type))


if __name__ == '__main__':
    # Test with a simple program
    import sys
    sys.path.insert(0, '.')
    from parser import parse_string
    
    test_code = '''
    [DATA_STORAGE]
    [GLOBAL]
        INT x = 42;
    
    [APPLICATION-STARTUP]
    function main() -> [VOID] {
        say(x);
        INT y = x + 10;
        say_f("Result: ", y);
    }
    '''
    
    ast = parse_string(test_code)
    print("Detailed AST:")
    print("=" * 60)
    print_detailed_ast(ast)
