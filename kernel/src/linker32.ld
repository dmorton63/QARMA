/* QuantumOS Kernel Linker Script */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* Kernel loaded at 1MB */
    . = 0x100000;
    
    /* Multiboot header */
    .multiboot :
    {
       KEEP(*(.multiboot))
    }
    
    /* Text section - executable code */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }
    
    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Data section - initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    /* BSS section - uninitialized data */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Quantum process table */
    .quantum_processes : ALIGN(4K)
    {
        quantum_process_table_start = .;
        . += 0x1000;  /* 4KB for quantum process table */
        quantum_process_table_end = .;
    }
    
    /* Parallel task queues */
    .parallel_queues : ALIGN(4K)
    {
        parallel_queue_start = .;
        . += 0x2000;  /* 8KB for parallel task queues */
        parallel_queue_end = .;
    }
    
    /* AI neural network data */
    .ai_networks : ALIGN(4K)
    {
        ai_network_start = .;
        . += 0x4000;  /* 16KB for AI neural networks */
        ai_network_end = .;
    }
    
    /* Security memory mapping tables */
    .security_tables : ALIGN(4K)
    {
        security_table_start = .;
        . += 0x1000;  /* 4KB for security tables */
        security_table_end = .;
    }
    
    /* Kernel heap */
    .heap : ALIGN(4K)
    {
        heap_start = .;
        . += 0x100000;  /* 1MB kernel heap */
        heap_end = .;
    }
    
    /* Kernel stack */
    .stack : ALIGN(4K)
    {
        stack_bottom = .;
        . += 0x10000;  /* 64KB kernel stack (increased from 16KB) */
        stack_top = .;
    }
    
    kernel_end = .;
}