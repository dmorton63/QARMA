/**
 * QARMA Kernel Panic Handler
 * 
 * Displays critical error messages and halts the system safely.
 */

#include "core/kernel.h"
#include "graphics/graphics.h"
#include "graphics/framebuffer.h"
#include "config.h"

// Panic state flag to prevent recursive panics
static bool in_panic = false;

/**
 * kernel_panic - Display critical error and halt system
 * 
 * This function:
 * 1. Disables interrupts to prevent further damage
 * 2. Clears the screen to a distinctive panic color
 * 3. Displays the error message prominently
 * 4. Halts the CPU in a loop requiring reboot
 * 
 * This function NEVER returns.
 */
void kernel_panic(const char* message) {
    // Prevent recursive panics
    if (in_panic) {
        __asm__ volatile("cli; hlt");
        while(1) { __asm__ volatile("hlt"); }
    }
    in_panic = true;
    
    // Disable interrupts immediately to prevent further issues
    __asm__ volatile("cli");
    
    // Log to serial for debugging
    extern void serial_debug(const char* msg);
    serial_debug("\n\n");
    serial_debug("====================================\n");
    serial_debug("        KERNEL PANIC\n");
    serial_debug("====================================\n");
    serial_debug(message);
    serial_debug("\n");
    serial_debug("System halted. Reboot required.\n");
    serial_debug("====================================\n");
    
    // Get framebuffer info
    extern uint32_t* backing_store;
    extern uint32_t fb_width, fb_height;
    
    if (backing_store && fb_width > 0 && fb_height > 0) {
        // Clear screen to red background (panic color)
        uint32_t panic_bg = 0xFFCC0000;  // Dark red
        for (uint32_t i = 0; i < fb_width * fb_height; i++) {
            backing_store[i] = panic_bg;
        }
        
        // Draw panic border (bright red)
        uint32_t border_color = 0xFFFF0000;  // Bright red
        for (uint32_t x = 0; x < fb_width; x++) {
            for (uint32_t y = 0; y < 10; y++) {
                backing_store[y * fb_width + x] = border_color;  // Top
                backing_store[(fb_height - 1 - y) * fb_width + x] = border_color;  // Bottom
            }
        }
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < 10; x++) {
                backing_store[y * fb_width + x] = border_color;  // Left
                backing_store[y * fb_width + (fb_width - 1 - x)] = border_color;  // Right
            }
        }
        
        // Draw panic header
        rgb_color_t white = {255, 255, 255, 255};
        rgb_color_t red_bg = {204, 0, 0, 255};
        
        int center_x = fb_width / 2;
        int start_y = 50;
        
        // Title
        const char* title = "*** KERNEL PANIC ***";
        int title_len = 0;
        while (title[title_len]) title_len++;
        int title_x = center_x - (title_len * 8) / 2;  // 8px per char
        
        gfx_draw_string(title_x, start_y, title, white, red_bg, NULL);
        
        // System halted message
        const char* halted = "System has encountered a critical error and must halt.";
        int halted_len = 0;
        while (halted[halted_len]) halted_len++;
        int halted_x = center_x - (halted_len * 8) / 2;
        
        gfx_draw_string(halted_x, start_y + 40, halted, white, red_bg, NULL);
        
        // Error message (word-wrapped if needed)
        int msg_y = start_y + 80;
        int msg_x = 50;  // Left margin
        
        const char* error_label = "Error: ";
        gfx_draw_string(msg_x, msg_y, error_label, white, red_bg, NULL);
        
        if (message) {
            // Simple single-line message display
            gfx_draw_string(msg_x, msg_y + 20, message, white, red_bg, NULL);
        }
        
        // Instructions
        const char* instr1 = "The system cannot continue and must be restarted.";
        const char* instr2 = "Please reboot your computer.";
        
        int instr_y = fb_height - 100;
        int instr1_len = 0, instr2_len = 0;
        while (instr1[instr1_len]) instr1_len++;
        while (instr2[instr2_len]) instr2_len++;
        
        gfx_draw_string(center_x - (instr1_len * 8) / 2, instr_y, instr1, white, red_bg, NULL);
        gfx_draw_string(center_x - (instr2_len * 8) / 2, instr_y + 20, instr2, white, red_bg, NULL);
        
        // Swap to display panic screen
        extern void framebuffer_swap(void);
        framebuffer_swap();
    } else {
        // Fallback: no graphics available, just use serial/text
        SERIAL_LOG("\n\n*** KERNEL PANIC ***\n");
        if (message) {
            SERIAL_LOG(message);
            SERIAL_LOG("\n");
        }
        SERIAL_LOG("System halted. Reboot required.\n\n");
    }
    
    // Halt the CPU forever
    // Use HLT instruction to reduce power consumption while halted
    while (1) {
        __asm__ volatile("hlt");
    }
}

/**
 * Assert macro implementation for kernel assertions
 */
void kernel_assert_failed(const char* expr, const char* file, int line) {
    extern void serial_debug(const char* msg);
    
    serial_debug("ASSERTION FAILED: ");
    serial_debug(expr);
    serial_debug(" at ");
    serial_debug(file);
    serial_debug(":");
    
    // Convert line number to string
    char line_buf[16];
    int i = 0;
    int n = line;
    do {
        line_buf[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    line_buf[i] = '\0';
    
    // Reverse the string
    for (int j = 0; j < i/2; j++) {
        char tmp = line_buf[j];
        line_buf[j] = line_buf[i-1-j];
        line_buf[i-1-j] = tmp;
    }
    
    serial_debug(line_buf);
    serial_debug("\n");
    
    // Build panic message
    char panic_msg[256];
    const char* prefix = "Assertion failed: ";
    int pos = 0;
    
    // Copy prefix
    while (*prefix && pos < 255) {
        panic_msg[pos++] = *prefix++;
    }
    
    // Copy expression (up to remaining space)
    while (*expr && pos < 255) {
        panic_msg[pos++] = *expr++;
    }
    
    panic_msg[pos] = '\0';
    
    kernel_panic(panic_msg);
}
