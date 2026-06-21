ENTRY(_start)

SECTIONS {
    . = 0x100000;

    .note.Xen ALIGN(4) : {
        *(.note.Xen)
    }
    
    .text ALIGN(4K) : {
        *(.multiboot)
        *(.text)
    }
    
    .rodata ALIGN(4K) : {
        *(.rodata)
    }
    
    .data ALIGN(4K) : {
        *(.data)
    }
    
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }
}
