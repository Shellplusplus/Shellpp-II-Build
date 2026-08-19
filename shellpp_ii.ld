/* Normalize Clang input sections to the names expected by NuttX modlib. */
SECTIONS
{
    .text : ALIGN(4)
    {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(4)
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    .init_array : ALIGN(4)
    {
        KEEP(*(.init_array))
        KEEP(*(.init_array.*))
    }

    .fini_array : ALIGN(4)
    {
        KEEP(*(.fini_array))
        KEEP(*(.fini_array.*))
    }

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx*)
    }

    /DISCARD/ :
    {
        *(.ARM.extab*)
        *(.comment)
        *(.llvm_addrsig)
        *(.note.GNU-stack)
    }
}
