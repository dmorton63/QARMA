#include "popup.h"
#include "core/input/input.h"
#include "core/text_functions/text.h"
#include "config.h"
#include <string.h>
#include "core/memory.h"
#include "core/memory/heap.h"
#include "keyboard/keyboard.h"
#include "core/io.h"


#define MAX_POPUP_WIDTH 300
#define LINE_HEIGHT 20
#define MAX_LINES 10


SavedRegion *save_region(int x, int y, int width, int height) {
    SavedRegion *region = heap_alloc(sizeof(SavedRegion));
    region->x = x;
    region->y = y;
    region->width = width;
    region->height = height;
    region->pixels = heap_alloc(width * height * sizeof(uint32_t));

    for (int j = 0; j < height; j++)
        for (int i = 0; i < width; i++)
            region->pixels[j * width + i] = fb_get_pixel(x + i, y + j);

    return region;
}

void restore_region(SavedRegion *region) {
    for (int j = 0; j < region->height; j++)
        for (int i = 0; i < region->width; i++)
            framebuffer_draw_pixel(region->x + i, region->y + j,
                          (rgb_color_t){
                              (region->pixels[j * region->width + i] >> 16) & 0xFF,
                              (region->pixels[j * region->width + i] >> 8) & 0xFF,
                              (region->pixels[j * region->width + i]) & 0xFF,
                              0xFF});

    heap_free(region->pixels);
    heap_free(region);
}

char **wrap_text(const char *message, int max_width, int *line_count) {
    char **lines = heap_alloc(sizeof(char *) * MAX_LINES);
    if (!lines) return NULL;
    int count = 0;
    const char *start = message;
    const char *end = message;

    while (*end && count < MAX_LINES) {
        while (*end && measure_text_pixel_width(start, end) < max_width)
            end++;

        const char *wrap = end;
        while (wrap > start && *wrap != ' ') wrap--;
        if (wrap == start) wrap = end;

        int len = wrap - start;
        lines[count] = heap_alloc(len + 1);
        if (!lines[count]) {
            // cleanup previous allocations
            for (int k = 0; k < count; ++k) heap_free(lines[k]);
            heap_free(lines);
            *line_count = 0;
            return NULL;
        }
        strncpy(lines[count], start, len);
        lines[count][len] = '\0';
        count++;

        start = (*wrap == ' ') ? wrap + 1 : wrap;
        end = start;
    }

    *line_count = count;
    return lines;
}

void show_popup(const struct popup_params *params) {
    int line_count = 0;
    gfx_print("Popup message: ");
    gfx_print(params->message);
    gfx_print("\nLine count: ");
    gfx_print_decimal(line_count);
    gfx_print("\n");

    char **wrapped = wrap_text(params->message, MAX_POPUP_WIDTH, &line_count);

    if (!wrapped) return; // allocation failure

    int width = MAX_POPUP_WIDTH + 20;
    int title_h = params->title ? (params->title_height > 0 ? params->title_height : 24) : 0;
    int height = title_h + (line_count * LINE_HEIGHT) + 40;

    SavedRegion *backdrop = save_region(params->x, params->y, width, height);

    // Fill background using framebuffer_draw_pixel so alpha is respected
    for (int yy = 0; yy < height; ++yy) {
        for (int xx = 0; xx < width; ++xx) {
            framebuffer_draw_pixel(params->x + xx, params->y + yy, params->bg_color);
        }
    }
    // Outline
    for (int i = 0; i < width; ++i) {
        framebuffer_draw_pixel(params->x + i, params->y, params->border_color);
        framebuffer_draw_pixel(params->x + i, params->y + height - 1, params->border_color);
    }
    for (int j = 0; j < height; ++j) {
        framebuffer_draw_pixel(params->x, params->y + j, params->border_color);
        framebuffer_draw_pixel(params->x + width - 1, params->y + j, params->border_color);
    }

    // Draw title bar if provided
    if (params->title) {
        // Title bar background
        for (int yy = 0; yy < title_h; ++yy) {
            for (int xx = 0; xx < width; ++xx) {
                framebuffer_draw_pixel(params->x + xx, params->y + yy, params->border_color);
            }
        }
        // Title text centered in title bar
    int title_text_w = measure_text_pixel_width(params->title, params->title + strlen(params->title));
    int title_x = params->x + (width - title_text_w) / 2;
    int title_y = params->y + (title_h - LINE_HEIGHT) / 2;
    fb_draw_text(title_x, title_y, (char*)params->title, params->title_color);
    }
    
    int text_x = params->x + 10;
    int text_y = params->y + title_h + ((height - title_h - (line_count * LINE_HEIGHT)) / 2);

    for (int i = 0; i < line_count; i++) {
        fb_draw_text(text_x, text_y + (i * LINE_HEIGHT), wrapped[i], params->text_color);
        heap_free(wrapped[i]);
    }

    heap_free(wrapped);

    if (params->timeout_ms > 0) {
        sleep_ms(params->timeout_ms);
        restore_region(backdrop);
        return;
    }

    // Blocking wait for a key scancode directly from the controller so
    // modal popups receive input even if the shell consumes buffered keys.
    // This bypasses higher-level buffering for the duration of the popup.

    // Prefer the interrupt-driven keyboard buffer. This keeps the IRQ-driven
    // path intact and avoids racing with the shell's input handling.
    while (1) {
        // Non-destructive: peek raw scancode first without consuming.
        uint8_t sc;
        if (keyboard_peek_scancode(&sc)) {
            // If prefix, peek next byte
            if (sc == 0xE0) {
                uint8_t next;
                if (!keyboard_peek_scancode_at(1, &next)) {
                    __asm__ volatile("hlt");
                    continue;
                }
                // If next is a release, wait
                if (next & KEY_RELEASE) {
                    __asm__ volatile("hlt");
                    continue;
                }

                if (next == KEY_ENTER) {
                    // consume both bytes (prefix + key)
                    (void)keyboard_get_scancode();
                    (void)keyboard_get_scancode();
                    if (params->on_confirm) params->on_confirm();
                    break;
                } else if (next == KEY_ESC) {
                    (void)keyboard_get_scancode();
                    (void)keyboard_get_scancode();
                    if (params->on_cancel) params->on_cancel();
                    break;
                } else {
                    // not a closing key, keep waiting
                    __asm__ volatile("hlt");
                    continue;
                }
            }

            // Non-prefix single-byte scancode
            if (sc & KEY_RELEASE) {
                __asm__ volatile("hlt");
                continue;
            }

            if (sc == KEY_ENTER) {
                // consume it
                (void)keyboard_get_scancode();
                if (params->on_confirm) params->on_confirm();
                break;
            } else if (sc == KEY_ESC) {
                (void)keyboard_get_scancode();
                if (params->on_cancel) params->on_cancel();
                break;
            }
        }

        // Fallback to ASCII buffer peek
        char c;
        if (keyboard_peek_char(&c)) {
            if (c == '\n' || c == '\r') {
                // consume and act
                (void)keyboard_get_char();
                if (params->on_confirm) params->on_confirm();
                break;
            } else if (c == 27) {
                (void)keyboard_get_char();
                if (params->on_cancel) params->on_cancel();
                break;
            }
        }

        __asm__ volatile("hlt");
    }

    restore_region(backdrop);
}

// Deprecated: this helper is replaced by show_popup + callbacks
// void show_popup_and_wait(const PopupParams* params) {
//     show_popup(params);
// }


void confirm_action(void) {
    // Show a small confirmation popup for 1 second
    PopupParams p = {
        .x = 120,
        .y = 120,
        .message = "Confirmed",
        .bg_color = (rgb_color_t){.red = 0x00, .green = 0x33, .blue = 0x00, .alpha = 0xFF},
        .border_color = (rgb_color_t){.red = 0x00, .green = 0xFF, .blue = 0x00, .alpha = 0xFF},
        .text_color = (rgb_color_t){.red = 0xFF, .green = 0xFF, .blue = 0xFF, .alpha = 0xFF},
        .timeout_ms = 1000,
        .dismiss_on_esc = false,
        .confirm_on_enter = false,
        .on_confirm = NULL,
        .on_cancel = NULL
    };
    show_popup(&p);
}

void cancel_action(void) {
    // Show a small cancellation popup for 1 second
    PopupParams p = {
        .x = 120,
        .y = 120,
        .message = "Cancelled",
        .bg_color = (rgb_color_t){.red = 0x33, .green = 0x00, .blue = 0x00, .alpha = 0xFF},
        .border_color = (rgb_color_t){.red = 0xFF, .green = 0x00, .blue = 0x00, .alpha = 0xFF},
        .text_color = (rgb_color_t){.red = 0xFF, .green = 0xFF, .blue = 0xFF, .alpha = 0xFF},
        .timeout_ms = 1000,
        .dismiss_on_esc = false,
        .confirm_on_enter = false,
        .on_confirm = NULL,
        .on_cancel = NULL
    };
    show_popup(&p);
}

// void popup_example(void) {
//     PopupParams params = {
//         .x = 100,
//         .y = 80,
//         .message = "Save changes?",
//         .bg_color = 0x222222,
//         .border_color = 0xFFFFFF,
//         .text_color = 0x00FF00,
//         .timeout_ms = 0,
//         .dismiss_on_esc = true,
//         .confirm_on_enter = true,
//         .on_confirm = confirm_action,
//         .on_cancel = cancel_action};
//     show_popup(&params);
// }