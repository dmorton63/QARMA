/* QARMA OS 64-bit Kernel Linker Script (higher-half VMA, low LMA) */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

/* Virtual and physical bases */
VMA_BASE = 0xFFFFFFFF80000000;
LMA_BASE = 0x00200000;   /* 2 MiB physical load address */

ENTRY(_start)

SECTIONS
{
  /* Keep the Multiboot2 header at the start of the file (first 32 KiB), 8-byte aligned */
  .multiboot 0 : ALIGN(8)
  {
    KEEP(*(.multiboot))
  }

  /* Move the location counter to the higher-half virtual address */
  . = VMA_BASE;

  /* Text */
  .text ALIGN(4K) : AT(ADDR(.text) - VMA_BASE + LMA_BASE)
  {
    *(.text .text.*)
  }

  /* Read-only data */
  .rodata ALIGN(4K) : AT(ADDR(.rodata) - VMA_BASE + LMA_BASE)
  {
    *(.rodata .rodata.*)
  }

  /* Data */
  .data ALIGN(4K) : AT(ADDR(.data) - VMA_BASE + LMA_BASE)
  {
    *(.data .data.*)
  }

  /* BSS (zero-initialized); still gets a load region but takes no file space */
  .bss ALIGN(4K) : AT(ADDR(.bss) - VMA_BASE + LMA_BASE)
  {
    *(COMMON)
    *(.bss .bss.*)
  }

  /* Optional runtime-reserved sections (don’t bloat file; place in .bss or initialize at boot) */
  .heap ALIGN(4K) : AT(ADDR(.heap) - VMA_BASE + LMA_BASE)
  {
    heap_start = .;
    . += 0x400000;  /* 4 MiB */
    heap_end = .;
  }

  .stack ALIGN(16) : AT(ADDR(.stack) - VMA_BASE + LMA_BASE)
  {
    stack_bottom = .;
    . += 0x20000;   /* 128 KiB */
    stack_top = .;
  }

  kernel_start = ADDR(.text);
  kernel_end   = .;

  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}