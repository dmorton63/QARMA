/**
 * Nomain Syntax Highlighter Implementation
 */

#include "nomain_syntax.h"
#include "memory.h"

// Character type helpers (since we can't use ctype.h)
static inline int isdigit(char c) { return c >= '0' && c <= '9'; }
static inline int isalpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int isalnum(char c) { return isdigit(c) || isalpha(c); }
static inline int isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// ============================================================================
// Color Definitions (VS Code Dark+ theme inspired)
// ============================================================================

#define COLOR_KEYWORD    0x00569CD6  // Blue
#define COLOR_TYPE       0x004EC9B0  // Cyan  
#define COLOR_TAG        0x00C586C0  // Purple
#define COLOR_BUILTIN    0x00DCDCAA  // Yellow
#define COLOR_STRING     0x00CE9178  // Orange
#define COLOR_CHAR       0x00CE9178  // Orange
#define COLOR_NUMBER     0x00B5CEA8  // Green
#define COLOR_COMMENT    0x006A9955  // Dark green
#define COLOR_OPERATOR   0x00D4D4D4  // Light gray
#define COLOR_IDENTIFIER 0x009CDCFE  // Light blue
#define COLOR_PUNCT      0x00D4D4D4  // Light gray
#define COLOR_WHITESPACE 0x00000000  // Transparent

// ============================================================================
// Helper Functions
// ============================================================================

static int string_match(const char* text, int pos, const char* pattern) {
    int i = 0;
    while (pattern[i] != '\0') {
        if (text[pos + i] != pattern[i]) {
            return 0;
        }
        i++;
    }
    // Check that the match ends at a word boundary
    char next = text[pos + i];
    if (next != '\0' && (isalnum(next) || next == '_' || next == ']')) {
        return 0;
    }
    return i;
}

static int is_word_char(char c) {
    return isalnum(c) || c == '_';
}

// ============================================================================
// Public API Implementation
// ============================================================================

uint32_t nomain_syntax_get_color(TokenType type) {
    switch (type) {
        case TOKEN_KEYWORD:     return COLOR_KEYWORD;
        case TOKEN_TYPE:        return COLOR_TYPE;
        case TOKEN_TAG:         return COLOR_TAG;
        case TOKEN_BUILTIN:     return COLOR_BUILTIN;
        case TOKEN_STRING:      return COLOR_STRING;
        case TOKEN_CHAR:        return COLOR_CHAR;
        case TOKEN_NUMBER:      return COLOR_NUMBER;
        case TOKEN_COMMENT:     return COLOR_COMMENT;
        case TOKEN_OPERATOR:    return COLOR_OPERATOR;
        case TOKEN_IDENTIFIER:  return COLOR_IDENTIFIER;
        case TOKEN_PUNCTUATION: return COLOR_PUNCT;
        case TOKEN_WHITESPACE:  return COLOR_WHITESPACE;
        default:                return COLOR_IDENTIFIER;
    }
}

bool nomain_syntax_is_keyword(const char* word, int length) {
    int keyword_count = sizeof(NOMAIN_KEYWORDS) / sizeof(NOMAIN_KEYWORDS[0]);
    for (int i = 0; i < keyword_count; i++) {
        const char* kw = NOMAIN_KEYWORDS[i];
        int kw_len = 0;
        while (kw[kw_len] != '\0') kw_len++;
        
        if (kw_len == length) {
            int match = 1;
            for (int j = 0; j < length; j++) {
                if (word[j] != kw[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

bool nomain_syntax_is_type(const char* word, int length) {
    int type_count = sizeof(NOMAIN_TYPES) / sizeof(NOMAIN_TYPES[0]);
    for (int i = 0; i < type_count; i++) {
        const char* type = NOMAIN_TYPES[i];
        int type_len = 0;
        while (type[type_len] != '\0') type_len++;
        
        if (type_len == length) {
            int match = 1;
            for (int j = 0; j < length; j++) {
                if (word[j] != type[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

bool nomain_syntax_is_builtin(const char* word, int length) {
    int builtin_count = sizeof(NOMAIN_BUILTINS) / sizeof(NOMAIN_BUILTINS[0]);
    for (int i = 0; i < builtin_count; i++) {
        const char* builtin = NOMAIN_BUILTINS[i];
        int builtin_len = 0;
        while (builtin[builtin_len] != '\0') builtin_len++;
        
        if (builtin_len == length) {
            int match = 1;
            for (int j = 0; j < length; j++) {
                if (word[j] != builtin[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

bool nomain_syntax_is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || 
           c == '=' || c == '<' || c == '>' || c == '!' ||
           c == '&' || c == '|' || c == '^' || c == '%';
}

bool nomain_syntax_is_punctuation(char c) {
    return c == '{' || c == '}' || c == '(' || c == ')' ||
           c == '[' || c == ']' || c == ';' || c == ',' ||
           c == '.' || c == ':';
}

Token* nomain_syntax_tokenize(const char* line, int* token_count) {
    // Allocate initial token array (will grow if needed)
    int capacity = 32;
    Token* tokens = malloc(capacity * sizeof(Token));
    *token_count = 0;
    
    int pos = 0;
    while (line[pos] != '\0' && line[pos] != '\n') {
        // Grow array if needed
        if (*token_count >= capacity) {
            capacity *= 2;
            Token* new_tokens = malloc(capacity * sizeof(Token));
            if (new_tokens) {
                for (int i = 0; i < *token_count; i++) {
                    new_tokens[i] = tokens[i];
                }
                free(tokens);
                tokens = new_tokens;
            }
        }
        
        Token* tok = &tokens[*token_count];
        tok->start = pos;
        
        // Check for tags: [DATA_STORAGE], [GLOBAL], etc.
        if (line[pos] == '[') {
            int tag_count = sizeof(NOMAIN_TAGS) / sizeof(NOMAIN_TAGS[0]);
            int matched = 0;
            for (int i = 0; i < tag_count; i++) {
                int len = string_match(line, pos, NOMAIN_TAGS[i]);
                if (len > 0) {
                    tok->type = TOKEN_TAG;
                    tok->length = len;
                    tok->color = nomain_syntax_get_color(TOKEN_TAG);
                    pos += len;
                    (*token_count)++;
                    matched = 1;
                    break;
                }
            }
            if (matched) continue;
            
            // Not a tag, treat as punctuation
            tok->type = TOKEN_PUNCTUATION;
            tok->length = 1;
            tok->color = nomain_syntax_get_color(TOKEN_PUNCTUATION);
            pos++;
            (*token_count)++;
            continue;
        }
        
        // Check for comments
        if (line[pos] == '/' && line[pos + 1] == '/') {
            tok->type = TOKEN_COMMENT;
            int start_pos = pos;
            while (line[pos] != '\0' && line[pos] != '\n') pos++;
            tok->length = pos - start_pos;
            tok->color = nomain_syntax_get_color(TOKEN_COMMENT);
            (*token_count)++;
            break;  // Rest of line is comment
        }
        
        // Check for strings
        if (line[pos] == '"') {
            tok->type = TOKEN_STRING;
            int start_pos = pos;
            pos++;  // Skip opening "
            while (line[pos] != '\0' && line[pos] != '"' && line[pos] != '\n') {
                if (line[pos] == '\\' && line[pos + 1] != '\0') {
                    pos += 2;  // Skip escape sequence
                } else {
                    pos++;
                }
            }
            if (line[pos] == '"') pos++;  // Skip closing "
            tok->length = pos - start_pos;
            tok->color = nomain_syntax_get_color(TOKEN_STRING);
            (*token_count)++;
            continue;
        }
        
        // Check for numbers
        if (isdigit(line[pos])) {
            tok->type = TOKEN_NUMBER;
            int start_pos = pos;
            while (isdigit(line[pos]) || line[pos] == '.') pos++;
            tok->length = pos - start_pos;
            tok->color = nomain_syntax_get_color(TOKEN_NUMBER);
            (*token_count)++;
            continue;
        }
        
        // Check for identifiers/keywords/types/builtins
        if (isalpha(line[pos]) || line[pos] == '_') {
            int start_pos = pos;
            while (is_word_char(line[pos])) pos++;
            int length = pos - start_pos;
            
            // Check if it's a keyword, type, or builtin
            if (nomain_syntax_is_keyword(&line[start_pos], length)) {
                tok->type = TOKEN_KEYWORD;
                tok->color = nomain_syntax_get_color(TOKEN_KEYWORD);
            } else if (nomain_syntax_is_type(&line[start_pos], length)) {
                tok->type = TOKEN_TYPE;
                tok->color = nomain_syntax_get_color(TOKEN_TYPE);
            } else if (nomain_syntax_is_builtin(&line[start_pos], length)) {
                tok->type = TOKEN_BUILTIN;
                tok->color = nomain_syntax_get_color(TOKEN_BUILTIN);
            } else {
                tok->type = TOKEN_IDENTIFIER;
                tok->color = nomain_syntax_get_color(TOKEN_IDENTIFIER);
            }
            
            tok->length = length;
            (*token_count)++;
            continue;
        }
        
        // Check for operators
        if (nomain_syntax_is_operator(line[pos])) {
            tok->type = TOKEN_OPERATOR;
            tok->length = 1;
            tok->color = nomain_syntax_get_color(TOKEN_OPERATOR);
            pos++;
            (*token_count)++;
            continue;
        }
        
        // Check for punctuation
        if (nomain_syntax_is_punctuation(line[pos])) {
            tok->type = TOKEN_PUNCTUATION;
            tok->length = 1;
            tok->color = nomain_syntax_get_color(TOKEN_PUNCTUATION);
            pos++;
            (*token_count)++;
            continue;
        }
        
        // Whitespace
        if (isspace(line[pos])) {
            tok->type = TOKEN_WHITESPACE;
            int start_pos = pos;
            while (isspace(line[pos]) && line[pos] != '\n') pos++;
            tok->length = pos - start_pos;
            tok->color = nomain_syntax_get_color(TOKEN_WHITESPACE);
            (*token_count)++;
            continue;
        }
        
        // Unknown character - treat as identifier
        tok->type = TOKEN_IDENTIFIER;
        tok->length = 1;
        tok->color = nomain_syntax_get_color(TOKEN_IDENTIFIER);
        pos++;
        (*token_count)++;
    }
    
    return tokens;
}

void nomain_syntax_free_tokens(Token* tokens) {
    if (tokens) {
        free(tokens);
    }
}
