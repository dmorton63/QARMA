"""
Abstract Syntax Tree (AST) Node Definitions for NOMAIN v0.1
These classes represent the structure of a parsed NOMAIN program
"""


class ASTNode:
    """Base class for all AST nodes"""
    def __repr__(self):
        return f"{self.__class__.__name__}({self.__dict__})"


# ============================================================================
# Program Structure
# ============================================================================

class Program(ASTNode):
    """Root node - represents entire NOMAIN program"""
    def __init__(self, data_storage, functions, startup_function):
        self.data_storage = data_storage      # DataStorage node
        self.functions = functions            # List of FunctionDef nodes
        self.startup_function = startup_function  # Name of startup function


class DataStorage(ASTNode):
    """Represents [DATA_STORAGE] section"""
    def __init__(self, declarations):
        self.declarations = declarations  # List of GlobalDeclaration nodes


class GlobalDeclaration(ASTNode):
    """Represents a variable declaration in [GLOBAL] section"""
    def __init__(self, var_type, name, initial_value=None, is_const=False):
        self.var_type = var_type          # Type name (INT, FLOAT, STRING, BOOL)
        self.name = name                  # Variable name
        self.initial_value = initial_value  # Initial value expression or None
        self.is_const = is_const          # True if CONST


# ============================================================================
# Functions
# ============================================================================

class FunctionDef(ASTNode):
    """Represents a function definition"""
    def __init__(self, name, parameters, return_type, body, is_startup=False):
        self.name = name                  # Function name
        self.parameters = parameters      # List of Parameter nodes
        self.return_type = return_type    # Return type or 'VOID'
        self.body = body                  # List of statement nodes
        self.is_startup = is_startup      # True if has [APPLICATION-STARTUP]


class Parameter(ASTNode):
    """Represents a function parameter"""
    def __init__(self, var_type, name):
        self.var_type = var_type  # Type name
        self.name = name          # Parameter name


# ============================================================================
# Statements
# ============================================================================

class AssignmentStmt(ASTNode):
    """Variable assignment: x = expression"""
    def __init__(self, variable, value):
        self.variable = variable  # Variable name
        self.value = value        # Expression node


class ReturnStmt(ASTNode):
    """RETURN statement"""
    def __init__(self, value=None):
        self.value = value  # Expression to return, or None for void


class IfStmt(ASTNode):
    """IF statement with optional ELSE"""
    def __init__(self, condition, then_block, else_block=None):
        self.condition = condition      # Expression node
        self.then_block = then_block    # List of statements
        self.else_block = else_block    # List of statements or None


class WhileStmt(ASTNode):
    """WHILE loop"""
    def __init__(self, condition, body):
        self.condition = condition  # Expression node
        self.body = body            # List of statements


class ForStmt(ASTNode):
    """FOR loop"""
    def __init__(self, init, condition, increment, body):
        self.init = init              # Initialization (VarDecl or Assignment)
        self.condition = condition    # Loop condition expression
        self.increment = increment    # Increment statement
        self.body = body              # List of statements


class VarDeclStmt(ASTNode):
    """Local variable declaration in function"""
    def __init__(self, var_type, name, initial_value=None):
        self.var_type = var_type          # Type name
        self.name = name                  # Variable name
        self.initial_value = initial_value  # Initial value or None


class ExpressionStmt(ASTNode):
    """Statement that's just an expression (like function call)"""
    def __init__(self, expression):
        self.expression = expression


# ============================================================================
# Expressions
# ============================================================================

class BinaryOp(ASTNode):
    """Binary operation: left op right"""
    def __init__(self, left, operator, right):
        self.left = left          # Left expression
        self.operator = operator  # Operator token type
        self.right = right        # Right expression


class UnaryOp(ASTNode):
    """Unary operation: op operand"""
    def __init__(self, operator, operand):
        self.operator = operator  # Operator token type
        self.operand = operand    # Expression


class FunctionCall(ASTNode):
    """Function call: func(arg1, arg2, ...)"""
    def __init__(self, name, arguments):
        self.name = name              # Function name
        self.arguments = arguments    # List of expression nodes


class Variable(ASTNode):
    """Variable reference"""
    def __init__(self, name):
        self.name = name  # Variable name


class Literal(ASTNode):
    """Literal value (number, string, boolean)"""
    def __init__(self, value, literal_type):
        self.value = value              # The actual value
        self.literal_type = literal_type  # Type (INT, FLOAT, STRING, BOOL)


# ============================================================================
# Helper Functions
# ============================================================================

def print_ast(node, indent=0):
    """Pretty print an AST for debugging"""
    prefix = "  " * indent
    
    if isinstance(node, Program):
        print(f"{prefix}Program:")
        print(f"{prefix}  DataStorage:")
        print_ast(node.data_storage, indent + 2)
        print(f"{prefix}  Functions:")
        for func in node.functions:
            print_ast(func, indent + 2)
        print(f"{prefix}  Startup: {node.startup_function}")
    
    elif isinstance(node, DataStorage):
        for decl in node.declarations:
            print_ast(decl, indent)
    
    elif isinstance(node, GlobalDeclaration):
        const_str = "CONST " if node.is_const else ""
        init_str = f" = {node.initial_value.value if isinstance(node.initial_value, Literal) else '...'}" if node.initial_value else ""
        print(f"{prefix}{const_str}{node.var_type} {node.name}{init_str}")
    
    elif isinstance(node, FunctionDef):
        startup_str = "[APPLICATION-STARTUP] " if node.is_startup else ""
        params_str = ", ".join(f"{p.var_type} {p.name}" for p in node.parameters)
        print(f"{prefix}{startup_str}function {node.name}({params_str}) -> {node.return_type}:")
        for stmt in node.body:
            print_ast(stmt, indent + 1)
    
    elif isinstance(node, AssignmentStmt):
        print(f"{prefix}{node.variable} = <expr>")
    
    elif isinstance(node, ReturnStmt):
        print(f"{prefix}RETURN <expr>")
    
    elif isinstance(node, IfStmt):
        print(f"{prefix}IF <condition> THEN")
        for stmt in node.then_block:
            print_ast(stmt, indent + 1)
        if node.else_block:
            print(f"{prefix}ELSE:")
            for stmt in node.else_block:
                print_ast(stmt, indent + 1)
    
    elif isinstance(node, WhileStmt):
        print(f"{prefix}WHILE <condition>:")
        for stmt in node.body:
            print_ast(stmt, indent + 1)
    
    elif isinstance(node, ForStmt):
        print(f"{prefix}FOR <init>; <condition>; <increment>:")
        for stmt in node.body:
            print_ast(stmt, indent + 1)
    
    elif isinstance(node, VarDeclStmt):
        init_str = " = <expr>" if node.initial_value else ""
        print(f"{prefix}{node.var_type} {node.name}{init_str}")
    
    elif isinstance(node, ExpressionStmt):
        print(f"{prefix}<expression_stmt>")
    
    elif isinstance(node, FunctionCall):
        args_str = ", ".join("..." for _ in node.arguments)
        print(f"{prefix}{node.name}({args_str})")
    
    else:
        print(f"{prefix}{node.__class__.__name__}")
