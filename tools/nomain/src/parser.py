"""
Parser for NOMAIN v0.1
Converts token stream into Abstract Syntax Tree (AST)
"""

from token_types import Token, TokenType
from ast_nodes import *


class ParseError(Exception):
    """Raised when parser encounters a syntax error"""
    def __init__(self, message, token):
        line = token.line if token else "?"
        col = token.column if token else "?"
        super().__init__(f"Parse error at line {line}, column {col}: {message}")
        self.token = token


class Parser:
    """Parses NOMAIN token stream into AST"""
    
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0
        self.current_token = self.tokens[0] if tokens else None
    
    def advance(self):
        """Move to next token"""
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
            self.current_token = self.tokens[self.pos]
    
    def peek(self, offset=1):
        """Look ahead at token"""
        pos = self.pos + offset
        if pos < len(self.tokens):
            return self.tokens[pos]
        return None
    
    def expect(self, token_type):
        """Consume token of expected type or raise error"""
        if self.current_token.type != token_type:
            raise ParseError(f"Expected {token_type}, got {self.current_token.type}", self.current_token)
        token = self.current_token
        self.advance()
        return token
    
    def match(self, *token_types):
        """Check if current token matches any of the given types"""
        return self.current_token and self.current_token.type in token_types
    
    # ========================================================================
    # Top Level Parsing
    # ========================================================================
    
    def parse(self):
        """Parse entire program"""
        # Parse [DATA_STORAGE] section
        data_storage = self.parse_data_storage()
        
        # Parse all functions
        functions = []
        startup_function = None
        
        while not self.match(TokenType.EOF):
            is_startup = False
            
            # Check for [APPLICATION-STARTUP] tag
            if self.match(TokenType.TAG_APPLICATION_STARTUP):
                if startup_function:
                    raise ParseError("Multiple [APPLICATION-STARTUP] tags found", self.current_token)
                is_startup = True
                self.advance()
            
            # Must have a function keyword
            if not self.match(TokenType.FUNCTION):
                break
                
            func = self.parse_function(is_startup)
            functions.append(func)
            
            if is_startup:
                startup_function = func.name
        
        if not startup_function:
            raise ParseError("No [APPLICATION-STARTUP] tag found", None)
        
        return Program(data_storage, functions, startup_function)
    
    # ========================================================================
    # Data Storage Section
    # ========================================================================
    
    def parse_data_storage(self):
        """Parse [DATA_STORAGE] section"""
        self.expect(TokenType.TAG_DATA_STORAGE)
        self.expect(TokenType.TAG_GLOBAL)
        
        declarations = []
        
        # Parse variable declarations until we hit 'function' or [APPLICATION-STARTUP]
        while not self.match(TokenType.FUNCTION, TokenType.TAG_APPLICATION_STARTUP, TokenType.EOF):
            decl = self.parse_global_declaration()
            declarations.append(decl)
        
        return DataStorage(declarations)
    
    def parse_global_declaration(self):
        """Parse a global variable declaration"""
        is_const = False
        
        if self.match(TokenType.CONST):
            is_const = True
            self.advance()
        
        # Get type
        if not self.match(TokenType.TYPE_INT, TokenType.TYPE_FLOAT, TokenType.TYPE_STRING, TokenType.TYPE_BOOL,
                         TokenType.TYPE_QUBIT, TokenType.TYPE_CIRCUIT, TokenType.TYPE_GATE, TokenType.TYPE_QSTATE):
            raise ParseError(f"Expected type, got {self.current_token.type}", self.current_token)
        
        var_type = self.current_token.value
        self.advance()
        
        # Get variable name
        name_token = self.expect(TokenType.IDENTIFIER)
        name = name_token.value
        
        # Optional initialization
        initial_value = None
        if self.match(TokenType.ASSIGN):
            self.advance()
            initial_value = self.parse_expression()
        
        self.expect(TokenType.SEMICOLON)
        
        return GlobalDeclaration(var_type, name, initial_value, is_const)
    
    # ========================================================================
    # Functions
    # ========================================================================
    
    def parse_function(self, is_startup=False):
        """Parse function definition"""
        self.expect(TokenType.FUNCTION)
        
        # Function name
        name_token = self.expect(TokenType.IDENTIFIER)
        name = name_token.value
        
        # Parameters
        self.expect(TokenType.LPAREN)
        parameters = []
        
        if not self.match(TokenType.RPAREN):
            parameters.append(self.parse_parameter())
            
            while self.match(TokenType.COMMA):
                self.advance()
                parameters.append(self.parse_parameter())
        
        self.expect(TokenType.RPAREN)
        
        # Return type
        self.expect(TokenType.ARROW)
        
        if self.match(TokenType.TAG_VOID):
            return_type = 'VOID'
            self.advance()
        else:
            if not self.match(TokenType.TYPE_INT, TokenType.TYPE_FLOAT, TokenType.TYPE_STRING, TokenType.TYPE_BOOL,
                             TokenType.TYPE_QUBIT, TokenType.TYPE_CIRCUIT, TokenType.TYPE_GATE, TokenType.TYPE_QSTATE):
                raise ParseError(f"Expected return type, got {self.current_token.type}", self.current_token)
            return_type = self.current_token.value
            self.advance()
        
        # Function body
        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)
        
        return FunctionDef(name, parameters, return_type, body, is_startup)
    
    def parse_parameter(self):
        """Parse function parameter"""
        if not self.match(TokenType.TYPE_INT, TokenType.TYPE_FLOAT, TokenType.TYPE_STRING, TokenType.TYPE_BOOL,
                         TokenType.TYPE_QUBIT, TokenType.TYPE_CIRCUIT, TokenType.TYPE_GATE, TokenType.TYPE_QSTATE):
            raise ParseError(f"Expected parameter type, got {self.current_token.type}", self.current_token)
        
        param_type = self.current_token.value
        self.advance()
        
        name_token = self.expect(TokenType.IDENTIFIER)
        name = name_token.value
        
        return Parameter(param_type, name)
    
    # ========================================================================
    # Statements
    # ========================================================================
    
    def parse_block(self):
        """Parse a block of statements"""
        statements = []
        
        while not self.match(TokenType.RBRACE, TokenType.EOF):
            stmt = self.parse_statement()
            if stmt:
                statements.append(stmt)
        
        return statements
    
    def parse_statement(self):
        """Parse a single statement"""
        # RETURN statement
        if self.match(TokenType.RETURN):
            return self.parse_return_statement()
        
        # IF statement
        if self.match(TokenType.IF):
            return self.parse_if_statement()
        
        # WHILE statement
        if self.match(TokenType.WHILE):
            return self.parse_while_statement()
        
        # FOR statement
        if self.match(TokenType.FOR):
            return self.parse_for_statement()
        
        # Variable declaration (starts with type)
        if self.match(TokenType.TYPE_INT, TokenType.TYPE_FLOAT, TokenType.TYPE_STRING, TokenType.TYPE_BOOL,
                     TokenType.TYPE_QUBIT, TokenType.TYPE_CIRCUIT, TokenType.TYPE_GATE, TokenType.TYPE_QSTATE):
            return self.parse_var_decl_statement()
        
        # Assignment or expression statement
        # This is tricky - need to look ahead to see if it's assignment
        if self.match(TokenType.IDENTIFIER):
            # Look ahead for assignment
            if self.peek() and self.peek().type == TokenType.ASSIGN:
                return self.parse_assignment_statement()
            else:
                # It's an expression (probably function call)
                return self.parse_expression_statement()
        
        raise ParseError(f"Unexpected token in statement: {self.current_token.type}", self.current_token)
    
    def parse_return_statement(self):
        """Parse RETURN statement"""
        self.expect(TokenType.RETURN)
        
        # Check if there's a return value
        value = None
        if not self.match(TokenType.SEMICOLON):
            value = self.parse_expression()
        
        self.expect(TokenType.SEMICOLON)
        return ReturnStmt(value)
    
    def parse_if_statement(self):
        """Parse IF statement"""
        self.expect(TokenType.IF)
        
        condition = self.parse_expression()
        
        self.expect(TokenType.THEN)
        
        # Parse THEN block
        if self.match(TokenType.LBRACE):
            self.advance()
            then_block = self.parse_block()
            self.expect(TokenType.RBRACE)
        else:
            # Single statement
            stmt = self.parse_statement()
            then_block = [stmt] if stmt else []
        
        # Optional ELSE block
        else_block = None
        if self.match(TokenType.ELSE):
            self.advance()
            
            if self.match(TokenType.LBRACE):
                self.advance()
                else_block = self.parse_block()
                self.expect(TokenType.RBRACE)
            else:
                # Single statement
                stmt = self.parse_statement()
                else_block = [stmt] if stmt else []
        
        return IfStmt(condition, then_block, else_block)
    
    def parse_while_statement(self):
        """Parse WHILE statement"""
        self.expect(TokenType.WHILE)
        
        condition = self.parse_expression()
        
        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)
        
        return WhileStmt(condition, body)
    
    def parse_for_statement(self):
        """Parse FOR statement"""
        self.expect(TokenType.FOR)
        
        # Initialization (variable declaration or assignment)
        if self.match(TokenType.TYPE_INT, TokenType.TYPE_FLOAT, TokenType.TYPE_STRING, TokenType.TYPE_BOOL,
                     TokenType.TYPE_QUBIT, TokenType.TYPE_CIRCUIT, TokenType.TYPE_GATE, TokenType.TYPE_QSTATE):
            init = self.parse_var_decl_statement()
        else:
            init = self.parse_assignment_statement()
        
        # Condition
        condition = self.parse_expression()
        self.expect(TokenType.SEMICOLON)
        
        # Increment (no semicolon at the end in FOR loops)
        var_name = self.expect(TokenType.IDENTIFIER).value
        self.expect(TokenType.ASSIGN)
        value = self.parse_expression()
        increment = AssignmentStmt(var_name, value)
        
        # Body
        self.expect(TokenType.LBRACE)
        body = self.parse_block()
        self.expect(TokenType.RBRACE)
        
        return ForStmt(init, condition, increment, body)
    
    def parse_var_decl_statement(self):
        """Parse local variable declaration"""
        var_type = self.current_token.value
        self.advance()
        
        name_token = self.expect(TokenType.IDENTIFIER)
        name = name_token.value
        
        # Optional initialization
        initial_value = None
        if self.match(TokenType.ASSIGN):
            self.advance()
            initial_value = self.parse_expression()
        
        self.expect(TokenType.SEMICOLON)
        
        return VarDeclStmt(var_type, name, initial_value)
    
    def parse_assignment_statement(self):
        """Parse assignment statement"""
        name_token = self.expect(TokenType.IDENTIFIER)
        variable = name_token.value
        
        self.expect(TokenType.ASSIGN)
        
        value = self.parse_expression()
        
        self.expect(TokenType.SEMICOLON)
        
        return AssignmentStmt(variable, value)
    
    def parse_expression_statement(self):
        """Parse expression as statement (e.g., function call)"""
        expr = self.parse_expression()
        self.expect(TokenType.SEMICOLON)
        return ExpressionStmt(expr)
    
    # ========================================================================
    # Expressions (with operator precedence)
    # ========================================================================
    
    def parse_expression(self):
        """Parse expression (logical OR has lowest precedence)"""
        return self.parse_logical_or()
    
    def parse_logical_or(self):
        """Parse logical OR"""
        left = self.parse_logical_and()
        
        while self.match(TokenType.OR):
            op = self.current_token.type
            self.advance()
            right = self.parse_logical_and()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_logical_and(self):
        """Parse logical AND"""
        left = self.parse_equality()
        
        while self.match(TokenType.AND):
            op = self.current_token.type
            self.advance()
            right = self.parse_equality()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_equality(self):
        """Parse equality operators"""
        left = self.parse_comparison()
        
        while self.match(TokenType.EQ, TokenType.NE):
            op = self.current_token.type
            self.advance()
            right = self.parse_comparison()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_comparison(self):
        """Parse comparison operators"""
        left = self.parse_addition()
        
        while self.match(TokenType.LT, TokenType.GT, TokenType.LE, TokenType.GE):
            op = self.current_token.type
            self.advance()
            right = self.parse_addition()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_addition(self):
        """Parse addition and subtraction"""
        left = self.parse_multiplication()
        
        while self.match(TokenType.PLUS, TokenType.MINUS):
            op = self.current_token.type
            self.advance()
            right = self.parse_multiplication()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_multiplication(self):
        """Parse multiplication and division"""
        left = self.parse_unary()
        
        while self.match(TokenType.MULTIPLY, TokenType.DIVIDE):
            op = self.current_token.type
            self.advance()
            right = self.parse_unary()
            left = BinaryOp(left, op, right)
        
        return left
    
    def parse_unary(self):
        """Parse unary operators"""
        if self.match(TokenType.NOT, TokenType.MINUS):
            op = self.current_token.type
            self.advance()
            operand = self.parse_unary()
            return UnaryOp(op, operand)
        
        return self.parse_primary()
    
    def parse_primary(self):
        """Parse primary expressions"""
        # Number literal
        if self.match(TokenType.NUMBER):
            value = self.current_token.value
            # Determine if INT or FLOAT
            literal_type = 'FLOAT' if isinstance(value, float) else 'INT'
            self.advance()
            return Literal(value, literal_type)
        
        # String literal
        if self.match(TokenType.STRING):
            value = self.current_token.value
            self.advance()
            return Literal(value, 'STRING')
        
        # Boolean literal
        if self.match(TokenType.BOOL):
            value = self.current_token.value
            self.advance()
            return Literal(value, 'BOOL')
        
        # Identifier (variable or function call)
        if self.match(TokenType.IDENTIFIER):
            name = self.current_token.value
            self.advance()
            
            # Check if it's a function call
            if self.match(TokenType.LPAREN):
                self.advance()
                
                # Parse arguments
                arguments = []
                if not self.match(TokenType.RPAREN):
                    arguments.append(self.parse_expression())
                    
                    while self.match(TokenType.COMMA):
                        self.advance()
                        arguments.append(self.parse_expression())
                
                self.expect(TokenType.RPAREN)
                return FunctionCall(name, arguments)
            else:
                # It's a variable
                return Variable(name)
        
        # Parenthesized expression
        if self.match(TokenType.LPAREN):
            self.advance()
            expr = self.parse_expression()
            self.expect(TokenType.RPAREN)
            return expr
        
        raise ParseError(f"Unexpected token in expression: {self.current_token.type}", self.current_token)


def parse_file(filename):
    """Parse a NOMAIN source file"""
    from lexer import tokenize_file
    tokens = tokenize_file(filename)
    parser = Parser(tokens)
    return parser.parse()


def parse_string(source):
    """Parse a NOMAIN source string"""
    from lexer import tokenize_string
    tokens = tokenize_string(source)
    parser = Parser(tokens)
    return parser.parse()


if __name__ == '__main__':
    # Test the parser
    test_code = '''
    [DATA_STORAGE]
    [GLOBAL]
        INT x = 42;
        STRING message = "Hello!";
    
    function add(INT a, INT b) -> INT {
        INT result = a + b;
        RETURN result;
    }
    
    [APPLICATION-STARTUP]
    function main() -> [VOID] {
        INT sum = add(5, 3);
        say_f("Sum: ", sum);
    }
    '''
    
    try:
        ast = parse_string(test_code)
        print("Parse successful!")
        print("\nAST Structure:")
        print("=" * 60)
        print_ast(ast)
    except (ParseError, Exception) as e:
        print(f"Error: {e}")
