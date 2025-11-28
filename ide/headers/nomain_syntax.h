/**
 * Nomain Syntax Highlighter
 * 
 * Tokenizes and colorizes nomain language source code
 */

#ifndef NOMAIN_SYNTAX_H
#define NOMAIN_SYNTAX_H

#include "core/stdtools.h"

// Token types
typedef enum {
    TOKEN_NONE,
    TOKEN_KEYWORD,      // IF, WHILE, FOR, RETURN, etc.
    TOKEN_TYPE,         // INT, FLOAT, STRING, BOOL
    TOKEN_TAG,          // [DATA_STORAGE], [GLOBAL], etc.
    TOKEN_BUILTIN,      // say, CreateQubit, etc.
    TOKEN_STRING,       // "text"
    TOKEN_CHAR,         // 'c'
    TOKEN_NUMBER,       // 123, 45.67
    TOKEN_COMMENT,      // // or /* */
    TOKEN_OPERATOR,     // +, -, *, /, =, etc.
    TOKEN_IDENTIFIER,   // variable/function names
    TOKEN_PUNCTUATION,  // { } ( ) ; ,
    TOKEN_WHITESPACE,
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    int start;
    int length;
    uint32_t color;
} Token;

// Nomain language keywords (.nom syntax)
static const char* NOMAIN_KEYWORDS[] = {
    "IF", "THEN", "ELSE",
    "WHILE", "FOR",
    "RETURN",
    "function",
    "CONST",
    "AND", "OR", "NOT",
    "true", "false"
};

// Nomain type keywords
static const char* NOMAIN_TYPES[] = {
    "INT", "FLOAT", "STRING", "BOOL",
    "QUBIT", "CIRCUIT", "GATE", "QSTATE"
};

// Nomain special tags
static const char* NOMAIN_TAGS[] = {
    "[DATA_STORAGE]",
    "[GLOBAL]",
    "[APPLICATION-STARTUP]",
    "[VOID]"
};

// Nomain built-in functions
static const char* NOMAIN_BUILTINS[] = {
    "say", "say_f",
    "INPUT", "INPUT_INT", "INPUT_FLOAT",
    "CreateQubit", "CreateQubits",
    "ApplyGate", "Measure", "GetState",
    "PLOT_BLOCH_SPHERE", "PLOT_PROBABILITIES",
    "SHOW_ENTANGLEMENT"
};

// ============================================================================
// Syntax Highlighting
// ============================================================================

/**
 * Tokenize a line of nomain code
 * Returns: array of tokens (caller must free)
 */
Token* nomain_syntax_tokenize(const char* line, int* token_count);

/**
 * Get color for token type
 */
uint32_t nomain_syntax_get_color(TokenType type);

/**
 * Check if word is a keyword
 */
bool nomain_syntax_is_keyword(const char* word, int length);

/**
 * Check if word is a type
 */
bool nomain_syntax_is_type(const char* word, int length);

/**
 * Check if character is operator
 */
bool nomain_syntax_is_operator(char c);

/**
 * Check if character is punctuation
 */
bool nomain_syntax_is_punctuation(char c);

/**
 * Free token array
 */
void nomain_syntax_free_tokens(Token* tokens);

#endif // NOMAIN_SYNTAX_H
