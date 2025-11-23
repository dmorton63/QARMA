"""
Lexer (Tokenizer) for NOMAIN v0.1
Converts source code into a stream of tokens
"""

import re
from token_types import Token, TokenType


class LexerError(Exception):
    """Raised when lexer encounters invalid input"""
    def __init__(self, message, line, column):
        super().__init__(f"Lexer error at line {line}, column {column}: {message}")
        self.line = line
        self.column = column


class Lexer:
    """Tokenizes NOMAIN source code"""
    
    # Keywords mapping
    KEYWORDS = {
        'function': TokenType.FUNCTION,
        'RETURN': TokenType.RETURN,
        'IF': TokenType.IF,
        'THEN': TokenType.THEN,
        'ELSE:': TokenType.ELSE,
        'WHILE': TokenType.WHILE,
        'FOR': TokenType.FOR,
        'AND': TokenType.AND,
        'OR': TokenType.OR,
        'NOT': TokenType.NOT,
        'CONST': TokenType.CONST,
        'true': TokenType.BOOL,
        'false': TokenType.BOOL,
        # Types
        'INT': TokenType.TYPE_INT,
        'FLOAT': TokenType.TYPE_FLOAT,
        'STRING': TokenType.TYPE_STRING,
        'BOOL': TokenType.TYPE_BOOL,
        # Quantum types
        'QUBIT': TokenType.TYPE_QUBIT,
        'CIRCUIT': TokenType.TYPE_CIRCUIT,
        'GATE': TokenType.TYPE_GATE,
        'QSTATE': TokenType.TYPE_QSTATE,
    }
    
    # Tags mapping
    TAGS = {
        '[DATA_STORAGE]': TokenType.TAG_DATA_STORAGE,
        '[GLOBAL]': TokenType.TAG_GLOBAL,
        '[APPLICATION-STARTUP]': TokenType.TAG_APPLICATION_STARTUP,
        '[VOID]': TokenType.TAG_VOID,
    }
    
    def __init__(self, source_code):
        self.source = source_code
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens = []
    
    def current_char(self):
        """Get current character or None if at end"""
        if self.pos >= len(self.source):
            return None
        return self.source[self.pos]
    
    def peek_char(self, offset=1):
        """Look ahead at next character(s)"""
        pos = self.pos + offset
        if pos >= len(self.source):
            return None
        return self.source[pos]
    
    def advance(self):
        """Move to next character"""
        if self.pos < len(self.source):
            if self.source[self.pos] == '\n':
                self.line += 1
                self.column = 1
            else:
                self.column += 1
            self.pos += 1
    
    def skip_whitespace(self):
        """Skip spaces and tabs (but not newlines in case we need them)"""
        while self.current_char() and self.current_char() in ' \t\r\n':
            self.advance()
    
    def skip_line_comment(self):
        """Skip // style comments"""
        # Skip the //
        self.advance()
        self.advance()
        # Skip until newline
        while self.current_char() and self.current_char() != '\n':
            self.advance()
    
    def skip_block_comment(self):
        """Skip /* */ style comments"""
        # Skip the /*
        self.advance()
        self.advance()
        
        while self.current_char():
            if self.current_char() == '*' and self.peek_char() == '/':
                self.advance()  # skip *
                self.advance()  # skip /
                return
            self.advance()
        
        raise LexerError("Unterminated block comment", self.line, self.column)
    
    def read_number(self):
        """Read a number (integer or float)"""
        start_line = self.line
        start_col = self.column
        num_str = ''
        has_dot = False
        
        while self.current_char() and (self.current_char().isdigit() or self.current_char() == '.'):
            if self.current_char() == '.':
                if has_dot:
                    raise LexerError("Invalid number: multiple decimal points", self.line, self.column)
                has_dot = True
            num_str += self.current_char()
            self.advance()
        
        value = float(num_str) if has_dot else int(num_str)
        return Token(TokenType.NUMBER, value, start_line, start_col)
    
    def read_string(self):
        """Read a string literal"""
        start_line = self.line
        start_col = self.column
        
        # Skip opening quote
        self.advance()
        
        string_value = ''
        while self.current_char() and self.current_char() != '"':
            if self.current_char() == '\\':
                # Handle escape sequences
                self.advance()
                if self.current_char() == 'n':
                    string_value += '\n'
                elif self.current_char() == 't':
                    string_value += '\t'
                elif self.current_char() == '"':
                    string_value += '"'
                elif self.current_char() == '\\':
                    string_value += '\\'
                else:
                    string_value += self.current_char()
                self.advance()
            else:
                string_value += self.current_char()
                self.advance()
        
        if not self.current_char():
            raise LexerError("Unterminated string", start_line, start_col)
        
        # Skip closing quote
        self.advance()
        
        return Token(TokenType.STRING, string_value, start_line, start_col)
    
    def read_identifier_or_keyword(self):
        """Read an identifier or keyword"""
        start_line = self.line
        start_col = self.column
        
        ident = ''
        while self.current_char() and (self.current_char().isalnum() or self.current_char() == '_'):
            ident += self.current_char()
            self.advance()
        
        # Special case: ELSE: (keyword with colon)
        if ident == 'ELSE' and self.current_char() == ':':
            ident += self.current_char()
            self.advance()
        
        # Check if it's a keyword
        token_type = self.KEYWORDS.get(ident, TokenType.IDENTIFIER)
        
        # Special handling for boolean literals
        if ident in ('true', 'false'):
            value = ident == 'true'
        else:
            value = ident
        
        return Token(token_type, value, start_line, start_col)
    
    def read_tag(self):
        """Read a [TAG] style token"""
        start_line = self.line
        start_col = self.column
        
        tag = ''
        while self.current_char() and self.current_char() != ']':
            tag += self.current_char()
            self.advance()
        
        if not self.current_char():
            raise LexerError("Unterminated tag", start_line, start_col)
        
        tag += self.current_char()  # Add closing ]
        self.advance()
        
        # Check if it's a known tag
        token_type = self.TAGS.get(tag, None)
        if not token_type:
            raise LexerError(f"Unknown tag: {tag}", start_line, start_col)
        
        return Token(token_type, tag, start_line, start_col)
    
    def tokenize(self):
        """Convert source code into list of tokens"""
        self.tokens = []
        
        while self.current_char():
            # Skip whitespace
            if self.current_char() in ' \t\r\n':
                self.skip_whitespace()
                continue
            
            # Comments
            if self.current_char() == '/' and self.peek_char() == '/':
                self.skip_line_comment()
                continue
            
            if self.current_char() == '/' and self.peek_char() == '*':
                self.skip_block_comment()
                continue
            
            # Tags
            if self.current_char() == '[':
                self.tokens.append(self.read_tag())
                continue
            
            # Numbers
            if self.current_char().isdigit():
                self.tokens.append(self.read_number())
                continue
            
            # Strings
            if self.current_char() == '"':
                self.tokens.append(self.read_string())
                continue
            
            # Identifiers and keywords
            if self.current_char().isalpha() or self.current_char() == '_':
                self.tokens.append(self.read_identifier_or_keyword())
                continue
            
            # Operators and punctuation
            start_line = self.line
            start_col = self.column
            ch = self.current_char()
            
            # Two-character operators
            if ch == '=' and self.peek_char() == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.EQ, '==', start_line, start_col))
                continue
            
            if ch == '!' and self.peek_char() == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.NE, '!=', start_line, start_col))
                continue
            
            if ch == '<' and self.peek_char() == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.LE, '<=', start_line, start_col))
                continue
            
            if ch == '>' and self.peek_char() == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.GE, '>=', start_line, start_col))
                continue
            
            if ch == '-' and self.peek_char() == '>':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.ARROW, '->', start_line, start_col))
                continue
            
            # Single-character tokens
            single_char_tokens = {
                '+': TokenType.PLUS,
                '-': TokenType.MINUS,
                '*': TokenType.MULTIPLY,
                '/': TokenType.DIVIDE,
                '=': TokenType.ASSIGN,
                '<': TokenType.LT,
                '>': TokenType.GT,
                ';': TokenType.SEMICOLON,
                ',': TokenType.COMMA,
                '(': TokenType.LPAREN,
                ')': TokenType.RPAREN,
                '{': TokenType.LBRACE,
                '}': TokenType.RBRACE,
            }
            
            if ch in single_char_tokens:
                token_type = single_char_tokens[ch]
                self.advance()
                self.tokens.append(Token(token_type, ch, start_line, start_col))
                continue
            
            # Unknown character
            raise LexerError(f"Unexpected character: '{ch}'", self.line, self.column)
        
        # Add EOF token
        self.tokens.append(Token(TokenType.EOF, None, self.line, self.column))
        return self.tokens


def tokenize_file(filename):
    """Tokenize a NOMAIN source file"""
    with open(filename, 'r') as f:
        source = f.read()
    
    lexer = Lexer(source)
    return lexer.tokenize()


def tokenize_string(source):
    """Tokenize a NOMAIN source string"""
    lexer = Lexer(source)
    return lexer.tokenize()


if __name__ == '__main__':
    # Test the lexer
    test_code = '''
    [DATA_STORAGE]
    [GLOBAL]
        INT x = 42;
        STRING message = "Hello, World!";
    
    function add(INT a, INT b) -> INT {
        RETURN a + b;
    }
    
    [APPLICATION-STARTUP]
    function main() -> [VOID] {
        INT result = add(5, 3);
        say_f("Result: ", result);
    }
    '''
    
    try:
        lexer = Lexer(test_code)
        tokens = lexer.tokenize()
        
        print("Tokens:")
        print("-" * 60)
        for token in tokens:
            print(token)
    except LexerError as e:
        print(f"Error: {e}")
