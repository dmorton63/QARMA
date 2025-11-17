#ifndef CONFIG_H
#define CONFIG_H

// Uncomment to enable specific debug channels
#define DEBUG_SERIAL
#define DEBUG_BOOTLOG
#define DEBUG_GRAPHICS
#define DEBUG_MEMORY
typedef enum {
    VERBOSITY_SILENT,
    VERBOSITY_MINIMAL,
    VERBOSITY_VERBOSE
} verbosity_level_t;

extern verbosity_level_t g_verbosity;

// Forward declare message box routing helpers so macros can call them
// without including graphics headers here.
void message_box_log(const char* msg);
void message_box_logf(const char* fmt, ...);


#ifdef DEBUG_SERIAL
    #define SERIAL_LOG(msg) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { serial_debug(msg); } \
    } while (0)

    #define SERIAL_LOG_MIN(msg) do { \
        if (g_verbosity >= VERBOSITY_MINIMAL) { serial_debug(msg); } \
    } while (0)

    #define SERIAL_LOG_HEX(prefix, val) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { \
            serial_debug(prefix); \
            serial_debug_hex(val); \
            serial_debug("\n"); \
        } \
    } while (0)

    #define SERIAL_LOG_DEC(prefix, val) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { \
            serial_debug(prefix); \
            serial_debug_decimal(val); \
            serial_debug("\n"); \
        } \
    } while (0)
#else
    #define SERIAL_LOG(msg) ((void)(msg))
    #define SERIAL_LOG_MIN(msg) ((void)(msg))
    #define SERIAL_LOG_HEX(prefix, val) ((void)(prefix), (void)(val))
    #define SERIAL_LOG_DEC(prefix, val) ((void)(prefix), (void)(val))
#endif

#ifdef DEBUG_BOOTLOG
    // BOOT_LOG buffers messages for later flushing. Do not directly call
    // message_box_log here because the boot log flush will call gfx_print()
    // which already routes into the message box. Calling both produced
    // duplicate entries in the message box.
    #define BOOT_LOG(msg) do { boot_log_push(msg); } while (0)
    #define BOOT_LOG_HEX(prefix, val) boot_log_push_hex(prefix, val)
    #define BOOT_LOG_DEC(prefix, val) boot_log_push_decimal(prefix, val)
#else
    #define BOOT_LOG(msg) ((void)(msg))
    #define BOOT_LOG_HEX(prefix, val) ((void)(prefix), (void)(val))
    #define BOOT_LOG_DEC(prefix, val) ((void)(prefix), (void)(val))
#endif





#ifdef DEBUG_GRAPHICS
    // gfx_print already routes into the message box; avoid calling
    // message_box_log twice which caused duplicated messages.
    #define GFX_LOG(msg) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { gfx_print(msg); } \
    } while (0)

    #define GFX_LOG_MIN(msg) do { \
        if (g_verbosity >= VERBOSITY_MINIMAL) { gfx_print(msg); } \
    } while (0)

    #define GFX_LOG_HEX(prefix, val) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { \
            gfx_print(prefix); \
            gfx_print_hex(val); \
            gfx_print("\n"); \
        } \
    } while (0)

    #define GFX_LOG_DEC(prefix, val) do { \
        if (g_verbosity >= VERBOSITY_VERBOSE) { \
            gfx_print(prefix); \
            gfx_print_decimal(val); \
            gfx_print("\n"); \
        } \
    } while (0)

#else
    #define GFX_LOG(msg) ((void)(msg))
    #define GFX_LOG_MIN(msg) ((void)(msg))
    #define GFX_LOG_HEX(prefix, val) ((void)(prefix), (void)(val))
    #define GFX_LOG_DEC(prefix, val) ((void)(prefix), (void)(val))
#endif

#endif /* CONFIG_H */
