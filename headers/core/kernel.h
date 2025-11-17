/**
 * QARMA Kernel - Core Header
 * 
 * Main header file defining kernel structures and interfaces
 */

#ifndef QARMA_KERNEL_H
#define QARMA_KERNEL_H

#include "kernel_types.h"
// Forward declarations
typedef struct multiboot_info multiboot_info_t;
typedef struct quantum_process quantum_process_t;
typedef struct parallel_context parallel_context_t;
typedef struct ai_context ai_context_t;

// Kernel state structure
typedef struct {
    bool initialized;
    uint32_t uptime_ticks;
    uint32_t quantum_processes;
    uint32_t parallel_tasks;
    uint32_t ai_agents;
} kernel_state_t;


enum vga_color {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LIGHT_GREY = 7,
    DARK_GREY = 8,
    LIGHT_BLUE = 9,
    LIGHT_GREEN = 10,
    LIGHT_CYAN = 11,
    LIGHT_RED = 12,
    LIGHT_MAGENTA = 13,
    LIGHT_BROWN = 14,
    WHITE = 15,
};

// External kernel state
extern kernel_state_t g_kernel_state;

int kernel_splash_test();
int kernel_login_test();

// Core kernel functions
int kernel_main(uint32_t multiboot_magic, multiboot_info_t* multiboot_info);
void kernel_early_init(void);
void kernel_print_banner(void);
int sprintf(char *buffer, const char *format, ...);
void memory_init(void);
void interrupts_init(void);
void drivers_init(void);
void scheduler_start(void);
void create_init_process(void);
void kernel_idle_loop(void);
void kernel_panic(const char* message);

void draw_splash(const char *title);

// Memory management
// void vmm_init(void);
// void pmm_init(void);
// void heap_init(void);
// void security_memory_init(void);
// void* heap_alloc(size_t size);
// void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
// bool vmm_map_framebuffer(uint32_t fb_physical_addr, uint32_t fb_size);

// Page flags
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4

// Interrupt system
void interrupts_system_init(void);
// void register_interrupt_handler(uint8_t int_no, isr_t handler);
void quantum_interrupts_init(void);
void idt_init(void);
void gdt_init(void);

// Console functions
void console_init(void);
void console_print(const char* str);
void console_print_hex(uint32_t value);

// Utility functions
void* memset(void* ptr, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
void kernel_delay(uint32_t count);

// Quantum system interfaces (defined in quantum_kernel.h)
void quantum_kernel_init(void);
void quantum_drivers_init(void);
void quantum_scheduler_init(void);
void quantum_scheduler_tick(void);
quantum_process_t* quantum_process_create(const char* name, uint32_t parent_qpid);

// Parallel processing interfaces (defined in parallel_engine.h)
void parallel_engine_init(void);
void parallel_drivers_init(void);
void parallel_scheduler_init(void);
void parallel_engine_tick(void);

// AI subsystem interfaces (defined in ai_subsystem.h)
void ai_subsystem_init(void);
void ai_hardware_init(void);
void ai_system_optimize(void);

// Security interfaces (defined in security_manager.h)
void security_manager_init(void);
void security_monitor_tick(void);


void serial_debug(const char* msg);
void serial_debug_hex(uint32_t value);
void serial_debug_decimal(uint32_t value);


#endif // QARMA_KERNEL_H