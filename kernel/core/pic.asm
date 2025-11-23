[BITS 64]
section .text
global init_pic

init_pic:
    ; Begin PIC initialization
    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    ; Set vector offsets
    mov al, 0x20        ; Master PIC → IRQ0–7 → vectors 32–39
    out 0x21, al
    mov al, 0x28        ; Slave PIC → IRQ8–15 → vectors 40–47
    out 0xA1, al

    ; Setup cascading
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    ; Set 8086/PC mode
    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    ; Unmask IRQ0 (timer) and IRQ1 (keyboard)
    mov al, 0xFC        ; 11111100 → mask all but IRQ0 and IRQ1
    out 0x21, al
    mov al, 0xFF        ; mask all on slave
    out 0xA1, al

    ret