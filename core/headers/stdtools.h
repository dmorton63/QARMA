
#ifndef STDTOOLS_H
#define STDTOOLS_H
// Fixed-width integer types
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

// Size type (optional if you use size_t in your kernel)
typedef uint32_t size_t;

typedef unsigned char  byte;   // 8 bits
typedef signed char    sbyte;  // 8 bits, explicitly signed
typedef unsigned int   dword;  // 32 bits on most systems
#ifdef ARCH_64
typedef unsigned long long uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif

#include "stdarg.h"

#include "stdbool.h"

// NULL
#define NULL ((void*)0)

#ifndef nullptr
#define nullptr ((void*)0)
#endif

#endif