#include "boot_log.h"
#include "graphics/graphics.h"
#include "string.h"
#include "config.h"

#define DEBUG_BUFFER_SIZE 4096

static char boot_log[BOOT_LOG_MAX_LINES][BOOT_LOG_LINE_LENGTH];
static uint32_t boot_log_count = 0;

static char debug_buffer[DEBUG_BUFFER_SIZE];
static size_t debug_index = 0;
#define MAX_DEBUG_LINES 128
#define MAX_LINE_LENGTH 128

static char debug_lines[MAX_DEBUG_LINES][MAX_LINE_LENGTH];
static size_t debug_line_count = 0;

void boot_log_push(const char *msg)
{
    if (boot_log_count < BOOT_LOG_MAX_LINES) {
        strncpy(boot_log[boot_log_count], msg, BOOT_LOG_LINE_LENGTH - 1);
        boot_log[boot_log_count][BOOT_LOG_LINE_LENGTH - 1] = '\0';
        boot_log_count++;
    }


}

void boot_log_push_hex(const char* label, uint32_t value) {
    if (boot_log_count >= BOOT_LOG_MAX_LINES) return;

    char buffer[BOOT_LOG_LINE_LENGTH];
    char hex_chars[] = "0123456789ABCDEF";

    // Copy label
    size_t label_len = strlen(label);
    if (label_len >= BOOT_LOG_LINE_LENGTH - 12) label_len = BOOT_LOG_LINE_LENGTH - 12;
    strncpy(buffer, label, label_len);
    buffer[label_len] = ' ';
    
    // Add "0x"
    buffer[label_len + 1] = '0';
    buffer[label_len + 2] = 'x';
    
    // Format value as hex
    for (int i = 7; i >= 0; i--) {
        buffer[label_len + 3 + (7 - i)] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    buffer[label_len + 11] = '\0';

    strncpy(boot_log[boot_log_count], buffer, BOOT_LOG_LINE_LENGTH - 1);
    boot_log[boot_log_count][BOOT_LOG_LINE_LENGTH - 1] = '\0';
    boot_log_count++;
}


void boot_log_flush(void){
    for (uint32_t i = 0; i < boot_log_count; i++) {
        gfx_print(boot_log[i]);
        gfx_print("\n");
    }
    boot_log_count = 0;

}


void boot_log_push_decimal(const char* label, uint32_t value) {
    if (boot_log_count >= BOOT_LOG_MAX_LINES) return;

    char buffer[BOOT_LOG_LINE_LENGTH];
    
    // Copy label
    size_t label_len = strlen(label);
    if (label_len >= BOOT_LOG_LINE_LENGTH - 15) label_len = BOOT_LOG_LINE_LENGTH - 15;
    strncpy(buffer, label, label_len);
    buffer[label_len] = ' ';
    
    // Convert number to string manually
    char num_str[12];
    itoa(value, num_str, 10);
    
    // Append number
    size_t num_len = strlen(num_str);
    strncpy(buffer + label_len + 1, num_str, num_len);
    buffer[label_len + 1 + num_len] = '\0';

    strncpy(boot_log[boot_log_count], buffer, BOOT_LOG_LINE_LENGTH - 1);
    boot_log[boot_log_count][BOOT_LOG_LINE_LENGTH - 1] = '\0';
    boot_log_count++;
}


void debug_buffer_clear() {
    debug_index = 0;
    debug_buffer[0] = '\0';
}

void debug_buffer_append(const char* msg) {
    size_t len = strlen(msg);
    if (debug_index + len < DEBUG_BUFFER_SIZE) {
        memcpy(&debug_buffer[debug_index], msg, len);
        debug_index += len;
        debug_buffer[debug_index] = '\0';
    }
}

void debug_buffer_flush() {
    SERIAL_LOG(debug_buffer);
    debug_buffer_clear();
}

void debug_buffer_append_hex(const char *label, uint32_t value) {
    char temp[64];
    char hex_chars[] = "0123456789ABCDEF";
    
    // Copy label
    size_t pos = 0;
    if (label) {
        while (*label && pos < 50) {
            temp[pos++] = *label++;
        }
    }
    
    // Add "0x"
    temp[pos++] = '0';
    temp[pos++] = 'x';
    
    // Add hex digits
    for (int i = 7; i >= 0; i--) {
        temp[pos++] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    temp[pos] = '\0';
    
    debug_buffer_append(temp);
}

void debug_buffer_append_dec(const char *label, uint32_t value) {
    char temp[64];
    
    // Copy label
    size_t pos = 0;
    if (label) {
        while (*label && pos < 50) {
            temp[pos++] = *label++;
        }
    }
    
    // Convert number to decimal string
    char num_str[12];
    int num_pos = 11;
    num_str[num_pos] = '\0';
    
    if (value == 0) {
        num_str[--num_pos] = '0';
    } else {
        while (value > 0) {
            num_str[--num_pos] = '0' + (value % 10);
            value /= 10;
        }
    }
    
    // Copy decimal string
    while (num_str[num_pos] && pos < 63) {
        temp[pos++] = num_str[num_pos++];
    }
    temp[pos] = '\0';
    
    debug_buffer_append(temp);
}


void debug_buffer_push(const char* msg) {
    if (debug_line_count < MAX_DEBUG_LINES) {
        strncpy(debug_lines[debug_line_count], msg, MAX_LINE_LENGTH - 1);
        debug_lines[debug_line_count][MAX_LINE_LENGTH - 1] = '\0';
        debug_line_count++;
    }
}

void debug_buffer_push_hex(const char *label, uint32_t value) {
    if (debug_line_count >= MAX_DEBUG_LINES) return;

    char temp[MAX_LINE_LENGTH];
    char hex_chars[] = "0123456789ABCDEF";
    
    // Copy label
    size_t pos = 0;
    if (label) {
        while (*label && pos < MAX_LINE_LENGTH - 12) {
            temp[pos++] = *label++;
        }
    }
    
    // Add "0x"
    temp[pos++] = '0';
    temp[pos++] = 'x';
    
    // Add hex digits
    for (int i = 7; i >= 0; i--) {
        temp[pos++] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    temp[pos] = '\0';
    
    debug_buffer_push(temp);
}

void debug_buffer_push_dec(const char *label, uint32_t value) {
    if (debug_line_count >= MAX_DEBUG_LINES) return;

    char temp[MAX_LINE_LENGTH];
    
    // Copy label
    size_t pos = 0;
    if (label) {
        while (*label && pos < MAX_LINE_LENGTH - 12) {
            temp[pos++] = *label++;
        }
    }
    
    // Convert number to decimal string
    char num_str[12];
    int num_pos = 11;
    num_str[num_pos] = '\0';
    
    if (value == 0) {
        num_str[--num_pos] = '0';
    } else {
        while (value > 0) {
            num_str[--num_pos] = '0' + (value % 10);
            value /= 10;
        }
    }
    
    // Copy decimal string
    while (num_str[num_pos] && pos < MAX_LINE_LENGTH - 1) {
        temp[pos++] = num_str[num_pos++];
    }
    temp[pos] = '\0';
    
    debug_buffer_push(temp);
}


void debug_buffer_flush_lines() {
    for (size_t i = 0; i < debug_line_count; i++) {
        SERIAL_LOG(debug_lines[i]);
        SERIAL_LOG("\n");
    }
    debug_line_count = 0;
}