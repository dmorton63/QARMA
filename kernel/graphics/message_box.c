#include "message_box.h"
#include "font_data.h"
#include "framebuffer.h"
#include "graphics/graphics.h"
//#include "config.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "core/text_functions/text.h"
#include "core/boot_log.h" // defines BOOT_LOG_LINE_LENGTH and kernel stdarg
#include <string.h>
#include "irq_logger.h"
#include "keyboard/command.h"
#include "keyboard/keyboard_types.h"

// Prevent routing loops: when the message box is actively rendering or
// pushing, avoid re-routing logs back into the box. Instead fall back to
// serial_debug to ensure debug output remains available.
static volatile int message_box_in_progress = 0;

extern void serial_debug(const char* msg);
static int scroll_offset = 0;
#define MAX_MESSAGES 32
#define LINE_HEIGHT 8

static char *messages[MAX_MESSAGES];
static int msg_head = 0; // oldest
static int msg_count = 0;
// Repeat counts for messages: when an identical message is pushed
// consecutively, we increment the repeat counter instead of adding
// another identical entry. This avoids visual floods from duplicate
// routing while keeping resource use low.
static int msg_repeats[MAX_MESSAGES];
static int box_height = 0;
static int screen_width = 0;
static int screen_height = 0;
// Early startup buffer: store messages produced before message_box_init()
#define EARLY_LINES 256
static char early_lines[EARLY_LINES][BOOT_LOG_LINE_LENGTH];
static int early_count = 0;
static int message_box_ready = 0;

static char current_input[BOOT_LOG_LINE_LENGTH];
static int input_cursor = 0;


void message_box_init(int height_px) {
    box_height = height_px;
    display_info_t* info = graphics_get_display_info();
    // Compute pixel dimensions using character-based info and known font size
    screen_width = info->width * 8; // FONT_WIDTH
    screen_height = info->height * 8; // FONT_HEIGHT
    SERIAL_LOG("MSGBOX: init called\n");
    SERIAL_LOG_DEC("MSGBOX: width chars=", info->width);
    SERIAL_LOG_DEC("MSGBOX: height chars=", info->height);
    SERIAL_LOG_DEC("MSGBOX: screen_px_w=", screen_width);
    SERIAL_LOG_DEC("MSGBOX: screen_px_h=", screen_height);
    // Mark ready and flush any queued early messages
    message_box_ready = 1;
    if (early_count > 0) {
        for (int i = 0; i < early_count; ++i) {
            // push without triggering recursion guard (we're not rendering yet)
            message_box_push(early_lines[i]);
        }
        early_count = 0;
    }
}

void message_box_push(const char* msg) {
    if (!msg) return;
    // If there is at least one message, check whether the new message
    // equals the most recent. If so, increment its repeat counter and
    // avoid pushing a duplicate entry.
    if (msg_count > 0) {
        int last_idx = (msg_head + msg_count - 1) % MAX_MESSAGES;
        if (messages[last_idx] && strcmp(messages[last_idx], msg) == 0) {
            // Bump repeat counter (saturate to a reasonable value)
            if (msg_repeats[last_idx] < 0x7fffffff) msg_repeats[last_idx]++;
            // Re-render so the UI can show the repeat collapsed (if desired)
            message_box_render();
            SERIAL_LOG("MSGBOX: duplicate message collapsed\n");
            return;
        }
    }
    // Simple ring buffer: replace oldest when full. Truncate long messages
    size_t len = strlen(msg);
    if (len > BOOT_LOG_LINE_LENGTH - 1) len = BOOT_LOG_LINE_LENGTH - 1;

    if (msg_count == MAX_MESSAGES) {
        // reuse memory: free oldest and replace
        heap_free(messages[msg_head]);
        messages[msg_head] = heap_alloc(len + 1);
        strncpy(messages[msg_head], msg, len);
        messages[msg_head][len] = '\0';
        msg_repeats[msg_head] = 0;
        msg_head = (msg_head + 1) % MAX_MESSAGES;
    } else {
        int idx = (msg_head + msg_count) % MAX_MESSAGES;
        messages[idx] = heap_alloc(len + 1);
        strncpy(messages[idx], msg, len);
        messages[idx][len] = '\0';
        msg_repeats[idx] = 0;
        msg_count++;
    }
    // Non-blocking: just render the box and return quickly
    message_box_render();
    SERIAL_LOG("MSGBOX: pushed message\n");
}

void message_box_push_norender(const char* msg) {
    if (!msg) return;
    // Duplicate of push but without rendering
    size_t len = strlen(msg);
    if (len > BOOT_LOG_LINE_LENGTH - 1) len = BOOT_LOG_LINE_LENGTH - 1;

    if (msg_count == MAX_MESSAGES) {
        heap_free(messages[msg_head]);
        messages[msg_head] = heap_alloc(len + 1);
        strncpy(messages[msg_head], msg, len);
        messages[msg_head][len] = '\0';
        msg_repeats[msg_head] = 0;
        msg_head = (msg_head + 1) % MAX_MESSAGES;
    } else {
        int idx = (msg_head + msg_count) % MAX_MESSAGES;
        messages[idx] = heap_alloc(len + 1);
        strncpy(messages[idx], msg, len);
        messages[idx][len] = '\0';
        msg_repeats[idx] = 0;
        msg_count++;
    }
    SERIAL_LOG("MSGBOX: pushed message (no render)\n");
}

void message_box_render(void) {
    display_info_t* info = graphics_get_display_info();
    if (!info) return;

    int px_width = info->width * 8;
    int px_height = info->height * 8;
    int box_y = px_height - box_height;

    // Draw background
    rgb_color_t bg = {0x11, 0x11, 0x11, 0xFF};
    for (int y = 0; y < box_height; ++y)
        for (int x = 0; x < px_width; ++x)
            framebuffer_draw_pixel(x, box_y + y, bg);

    // Compute visible lines
    int max_lines = box_height / LINE_HEIGHT - 1; // reserve one line for input
    if (max_lines < 1) max_lines = 1;
    int lines = msg_count > max_lines ? max_lines : msg_count;
    // Clamp scroll_offset: it cannot be negative and cannot exceed the number
    // of messages that are off-screen (msg_count - lines)
    if (scroll_offset < 0) scroll_offset = 0;
    int max_scroll = (msg_count > lines) ? (msg_count - lines) : 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    // Compute start index safely (avoid negative modulo)
    int start_index = msg_head + (msg_count - lines - scroll_offset);
    // Normalize into [0, MAX_MESSAGES)
    while (start_index < 0) start_index += MAX_MESSAGES;
    int start = start_index % MAX_MESSAGES;

    // Render messages
    rgb_color_t fg = {0xFF, 0xFF, 0xFF, 0xFF};
    rgb_color_t text_bg = bg;
    int base_y = box_y + 2;
    for (int i = 0; i < lines; ++i) {
        int idx = (start + i) % MAX_MESSAGES;
        const char* s = messages[idx];
        int x = 4;
        int y = base_y + i * LINE_HEIGHT;
        for (int k = 0; s[k]; ++k)
            framebuffer_draw_char(x + k * 8, y, s[k], fg, text_bg);
    }

    // Render input line
    int input_y = base_y + lines * LINE_HEIGHT;
    framebuffer_draw_char(4, input_y, '>', fg, text_bg);
    for (int k = 0; current_input[k]; ++k)
        framebuffer_draw_char(12 + k * 8, input_y, current_input[k], fg, text_bg);

    // Optional: blinking cursor
    framebuffer_draw_char(12 + input_cursor * 8, input_y, '_', fg, text_bg);
}

void message_box_scroll_up(int lines) {
    if (lines <= 0) return;
    scroll_offset += lines;
    message_box_render();
}

void message_box_scroll_down(int lines) {
    if (lines <= 0) return;
    scroll_offset -= lines;
    if (scroll_offset < 0) scroll_offset = 0;
    message_box_render();
}

void message_box_scroll_top(void) {
    // Scroll to show the oldest messages (maximum scroll)
    int max_lines = box_height / LINE_HEIGHT - 1;
    if (max_lines < 1) max_lines = 1;
    int max_scroll = (msg_count > max_lines) ? (msg_count - max_lines) : 0;
    scroll_offset = max_scroll;
    message_box_render();
}

void message_box_scroll_bottom(void) {
    scroll_offset = 0;
    message_box_render();
}

void message_box_page_up(void) {
    int max_lines = box_height / LINE_HEIGHT - 1;
    if (max_lines < 1) max_lines = 1;
    message_box_scroll_up(max_lines);
}

void message_box_page_down(void) {
    int max_lines = box_height / LINE_HEIGHT - 1;
    if (max_lines < 1) max_lines = 1;
    message_box_scroll_down(max_lines);
}

// Map a raw keycode to a printable character. For now we support
// direct ASCII keycodes; extended keys return 0. This keeps the
// message box independent of keyboard internals and avoids a
// hard dependency on a missing helper elsewhere.
static char keycode_to_char(int keycode) {
    if (keycode >= 32 && keycode < 127) return (char)keycode;
    return 0;
}

void message_box_log(const char* msg) {
    if (!msg) return;
    if (message_box_in_progress) {
        // Avoid recursion: fallback to serial output
        serial_debug(msg);
        return;
    }
    // If message box isn't ready yet, buffer the message for later
    if (!message_box_ready) {
        if (early_count < EARLY_LINES) {
            size_t len = strlen(msg);
            if (len >= BOOT_LOG_LINE_LENGTH) len = BOOT_LOG_LINE_LENGTH - 1;
            strncpy(early_lines[early_count], msg, len);
            early_lines[early_count][len] = '\0';
            early_count++;
        } else {
            // Overflow: fallback to serial
            serial_debug(msg);
        }
        return;
    }

    message_box_in_progress = 1;
    // Route a single line into the message box. This is light-weight and
    // intended to be safe to call from many places. It does not allocate
    // excessively; long messages are truncated.
    message_box_push(msg);
    message_box_in_progress = 0;
}

// Minimal formatter supporting %s, %d, %u, %x, %c, %p
static void tiny_vformat(char *out, size_t outsz, const char *fmt, va_list ap) {
    size_t pos = 0;
    const char *p = fmt;
    while (*p && pos + 1 < outsz) {
        if (*p != '%') {
            out[pos++] = *p++;
            continue;
        }
        p++; // skip '%'
        if (*p == 's') {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && pos + 1 < outsz) out[pos++] = *s++;
        } else if (*p == 'd' || *p == 'u') {
            unsigned int v = va_arg(ap, unsigned int);
            char buf[12];
            int bp = 0;
            if (*p == 'd' && (int)v == 0) {
                buf[bp++] = '0';
            } else if (*p == 'd') {
                int vi = (int)v;
                if (vi < 0) {
                    if (pos + 1 < outsz) out[pos++] = '-';
                    vi = -vi;
                }
                unsigned int uv = (unsigned int)vi;
                while (uv > 0 && bp < (int)sizeof(buf)-1) {
                    buf[bp++] = '0' + (uv % 10);
                    uv /= 10;
                }
                if (bp == 0) buf[bp++] = '0';
            } else { // unsigned
                unsigned int uv = v;
                while (uv > 0 && bp < (int)sizeof(buf)-1) {
                    buf[bp++] = '0' + (uv % 10);
                    uv /= 10;
                }
                if (bp == 0) buf[bp++] = '0';
            }
            // reverse
            for (int i = bp - 1; i >= 0 && pos + 1 < outsz; --i) out[pos++] = buf[i];
        } else if (*p == 'x' || *p == 'p') {
            unsigned int v = va_arg(ap, unsigned int);
            char hex[] = "0123456789ABCDEF";
            char buf[9];
            int bp = 0;
            if (v == 0) {
                buf[bp++] = '0';
            } else {
                while (v && bp < 8) {
                    buf[bp++] = hex[v & 0xF];
                    v >>= 4;
                }
            }
            // reverse
            for (int i = bp - 1; i >= 0 && pos + 1 < outsz; --i) out[pos++] = buf[i];
        } else if (*p == 'c') {
            int c = va_arg(ap, int);
            out[pos++] = (char)c;
        } else {
            // Unknown specifier, emit literally
            out[pos++] = '%';
            if (pos + 1 < outsz) out[pos++] = *p;
        }
        p++;
    }
    out[pos] = '\0';
}

void message_box_logf(const char* fmt, ...) {
    if (!fmt) return;
    char buf[BOOT_LOG_LINE_LENGTH];
    va_list ap;
    if (message_box_in_progress) {
        // Format and fallback to serial to avoid recursion
        va_start(ap, fmt);
        tiny_vformat(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        serial_debug(buf);
        return;
    }
    message_box_in_progress = 1;
    va_start(ap, fmt);
    tiny_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    message_box_log(buf);
    message_box_in_progress = 0;
}

int message_box_is_ready(void) {
    return message_box_ready;
}

void message_box_handle_key(int keycode) {
    switch (keycode) {
        case KEY_UP: message_box_scroll_up(1); break;
        case KEY_DOWN: message_box_scroll_down(1); break;
        case KEY_PGUP: message_box_page_up(); break;
        case KEY_PGDN: message_box_page_down(); break;
        case KEY_ENTER:
            message_box_push(current_input); // echo command
            execute_command(current_input);  // run it
            current_input[0] = '\0';
            input_cursor = 0;
            scroll_offset = 0;
            break;
        case KEY_BACKSPACE:
            if (input_cursor > 0) current_input[--input_cursor] = '\0';
            break;
        default:
            if (input_cursor < BOOT_LOG_LINE_LENGTH - 1) {
                current_input[input_cursor++] = keycode_to_char(keycode);
                current_input[input_cursor] = '\0';
            }
            break;
    }
    message_box_render();
}

// Note: command execution is provided by kernel/keyboard/command.c