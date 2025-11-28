/**
 * QARMA String Library
 * 
 * Standard C string functions for freestanding kernel environment.
 * Provides all essential string manipulation functions without libc dependency.
 */

#ifndef STRING_H
#define STRING_H

#include "kernel_types.h"

// String length and comparison
size_t strlen(const char* str);
size_t strnlen(const char* str, size_t maxlen);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
int strcasecmp(const char* str1, const char* str2);
int strncasecmp(const char* str1, const char* str2, size_t n);

// String copying and concatenation
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);

// String searching
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);
char* strstr(const char* haystack, const char* needle);
char* strpbrk(const char* str, const char* accept);
size_t strspn(const char* str, const char* accept);
size_t strcspn(const char* str, const char* reject);

// String tokenization
char* strtok(char* str, const char* delim);
char* strtok_r(char* str, const char* delim, char** saveptr);

// Memory functions
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
void* memmove(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
void* memchr(const void* ptr, int value, size_t num);

// Memory allocation utilities (safe versions)
void* memzero(void* ptr, size_t size);
bool memeq(const void* ptr1, const void* ptr2, size_t size);

// String conversion functions
int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);
unsigned long strtoul(const char* str, char** endptr, int base);
long strtol(const char* str, char** endptr, int base);

// Number to string conversion
char* itoa(int value, char* str, int base);
char* ltoa(long value, char* str, int base);
char* ultoa(unsigned long value, char* str, int base);
char* lltoa(long long value, char* str, int base);

// Utility functions for kernel strings (basic implementations)
// Note: These are simplified versions for kernel use
int sprintf(char* buffer, const char* format, ...);
int vsprintf(char *buffer, const char *format, va_list args);
int snprintf(char *buffer, size_t size, const char *format, ...);

// String manipulation utilities
char* strdup(const char* str);  // Note: requires memory allocator
char* strndup(const char* str, size_t n);  // Note: requires memory allocator
void strrev(char* str);
char* strtrim(char* str);
char* strlwr(char* str);
char* strupr(char* str);

// Safe string functions (bounds checking)
size_t strlcpy(char* dest, const char* src, size_t size);
size_t strlcat(char* dest, const char* src, size_t size);

// Character classification (simple versions)
static inline int isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static inline int isalnum(int c) { return isalpha(c) || isdigit(c); }
static inline int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
static inline int isupper(int c) { return c >= 'A' && c <= 'Z'; }
static inline int islower(int c) { return c >= 'a' && c <= 'z'; }
static inline int isprint(int c) { return c >= 32 && c <= 126; }
static inline int isxdigit(int c) { return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

// Character conversion
static inline int toupper(int c) { return islower(c) ? c - 32 : c; }
static inline int tolower(int c) { return isupper(c) ? c + 32 : c; }

// Kernel-specific string utilities
void hexdump(const void* data, size_t size);
char* bin2hex(const void* data, size_t size, char* buffer);
int hex2bin(const char* hex, void* buffer, size_t max_size);
bool is_valid_utf8(const char* str);

#endif // STRING_H