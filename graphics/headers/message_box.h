#pragma once

#include "core/stdtools.h"
#include "framebuffer.h"
#include "core/boot_log.h" // canonical BOOT_LOG_LINE_LENGTH
#define MAX_MESSAGES 32
#define MAX_CMD_HISTORY 64
#define LINE_HEIGHT 8

// Message entry structure (for future use)
typedef struct {
    char* text;
    int repeat_count;
    uint32_t timestamp_ticks;
    enum { MSG_INFO, MSG_WARN, MSG_ERROR, MSG_CMD } type;
} message_entry_t;

// Initialize the message box area (height in pixels)
void message_box_init(int height_px);

// Push a single line (null-terminated) to the message box
void message_box_push(const char* msg);

// Like message_box_push but does not trigger render. Useful for
// bulk-flushing preformatted messages (e.g., from IRQ logger) to avoid
// recursion when called from message_box_render().
void message_box_push_norender(const char* msg);

// Render/update the message box (called automatically by push)
void message_box_render(void);

// Route arbitrary log text into the message box (thread-safe-ish)
// This accepts a single null-terminated line and will be truncated
// if it exceeds the box width. Use this from debug macros to show
// messages on the desktop UI.
void message_box_log(const char* msg);

// Convenience formatted logger (very small snprintf-like implementation)
// Note: limited to 256 bytes per call to avoid large stack usage.
void message_box_logf(const char* fmt, ...);

// Return non-zero when the message box has been initialized and is ready
// to accept logs. Useful for callers that want to route prints only to
// the message box instead of drawing to the desktop framebuffer.
int message_box_is_ready(void);

// Scrolling helpers: move the viewport up/down by the specified number of
// message lines (pass 1 to move a single line). "Up" moves older messages
// into view (i.e. scrolls history up). The functions clamp automatically
// against available history and trigger a redraw.
void message_box_scroll_up(int lines);
void message_box_scroll_down(int lines);
void message_box_scroll_top(void);
void message_box_scroll_bottom(void);

// Page-level scrolling (scroll by visible lines)
void message_box_page_up(void);
void message_box_page_down(void);
