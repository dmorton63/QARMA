"""
Token types for NOMAIN v0.1 Lexer
"""

from enum import Enum, auto

class TokenType(Enum):
    # Literals
    NUMBER = auto()          # 42, 3.14
    STRING = auto()          # "hello"
    BOOL = auto()            # true, false
    
    # Identifiers and Keywords
    IDENTIFIER = auto()      # variable names, function names
    
    # Keywords
    FUNCTION = auto()        # function
    RETURN = auto()          # RETURN
    IF = auto()              # IF
    THEN = auto()            # THEN
    ELSE = auto()            # ELSE:
    WHILE = auto()           # WHILE
    FOR = auto()             # FOR
    AND = auto()             # AND
    OR = auto()              # OR
    NOT = auto()             # NOT
    CONST = auto()           # CONST
    
    # Types
    TYPE_INT = auto()        # INT
    TYPE_FLOAT = auto()      # FLOAT
    TYPE_STRING = auto()     # STRING
    TYPE_BOOL = auto()       # BOOL
    TYPE_QUBIT = auto()      # QUBIT (quantum)
    TYPE_CIRCUIT = auto()    # CIRCUIT (quantum)
    TYPE_GATE = auto()       # GATE (quantum)
    TYPE_QSTATE = auto()     # QSTATE (quantum)
    
    # Tags
    TAG_DATA_STORAGE = auto()        # [DATA_STORAGE]
    TAG_GLOBAL = auto()              # [GLOBAL]
    TAG_APPLICATION_STARTUP = auto() # [APPLICATION-STARTUP]
    TAG_VOID = auto()                # [VOID]
    
    # Operators
    PLUS = auto()            # +
    MINUS = auto()           # -
    MULTIPLY = auto()        # *
    DIVIDE = auto()          # /
    ASSIGN = auto()          # =
    
    # Comparison
    EQ = auto()              # ==
    NE = auto()              # !=
    LT = auto()              # <
    GT = auto()              # >
    LE = auto()              # <=
    GE = auto()              # >=
    
    # Punctuation
    SEMICOLON = auto()       # ;
    COMMA = auto()           # ,
    LPAREN = auto()          # (
    RPAREN = auto()          # )
    LBRACE = auto()          # {
    RBRACE = auto()          # }
    ARROW = auto()           # ->
    
    # Special
    NEWLINE = auto()
    EOF = auto()
    
    def __repr__(self):
        return self.name


class Token:
    """Represents a single token"""
    def __init__(self, type, value, line, column):
        self.type = type
        self.value = value
        self.line = line
        self.column = column
    
    def __repr__(self):
        return f"Token({self.type}, {repr(self.value)}, {self.line}:{self.column})"
    
    def __str__(self):
        return f"{self.type}({self.value})"
