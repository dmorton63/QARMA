/* QARMA OS 64-bit Kernel Linker Script */
/* Higher-half kernel at -2GB (0xFFFFFFFF80000000) */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    /* Kernel loaded physically at 2MB to avoid low memory */
    /* But linked to run at higher-half address */
    . = 0xFFFFFFFF80000000 + 0x200000;
    
    kernel_start = .;
    
    /* Multiboot header must be in first 8KB and 8-byte aligned */
    .multiboot ALIGN(8) :
    {
       KEEP(*(.multiboot))
    }
    
    /* Text section - executable code */
    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }
    
    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Data section - initialized data */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }
    
    /* BSS section - uninitialized data */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Quantum process table */
    .quantum_processes ALIGN(4K) :
    {
        quantum_process_table_start = .;
        . += 0x1000;  /* 4KB for quantum process table */
        quantum_process_table_end = .;
    }
    
    /* Parallel task queues */
    .parallel_queues ALIGN(4K) :
    {
        parallel_queue_start = .;
        . += 0x2000;  /* 8KB for parallel task queues */
        parallel_queue_end = .;
    }
    
    /* AI neural network data */
    .ai_networks ALIGN(4K) :
    {
        ai_network_start = .;
        . += 0x4000;  /* 16KB for AI neural networks */
        ai_network_end = .;
    }
    
    /* Security memory mapping tables */
    .security_tables ALIGN(4K) :
    {
        security_table_start = .;
        . += 0x1000;  /* 4KB for security tables */
        security_table_end = .;
    }
    
    /* Kernel heap */
    .heap ALIGN(4K) :
    {
        heap_start = .;
        . += 0x400000;  /* 4MB kernel heap (increased for 64-bit) */
        heap_end = .;
    }
    
    /* Kernel stack */
    .stack ALIGN(16) :
    {
        stack_bottom = .;
        . += 0x20000;  /* 128KB kernel stack (64-bit needs more) */
        stack_top = .;
    }
    
    kernel_end = .;
    
    /* Discard unneeded sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
