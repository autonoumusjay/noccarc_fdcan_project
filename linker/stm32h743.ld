/* STM32H743 Bare-Metal Linker Script
 *
 * Memory configuration:
 * FLASH: 0x08000000, 2 MB
 * RAM:   0x20000000, 128 KB (DTCM)
 *
 * Note:
 * This is a starter memory layout for the current project.
 * DMA buffers may need to be placed in DMA-accessible SRAM instead.
 */

ENTRY(Reset_Handler)

/* Heap and stack reservations */
_Min_Heap_Size  = 0x400;  /* 1 KB */
_Min_Stack_Size = 0x800;  /* 2 KB */

/* Memory regions */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Initial stack pointer */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* Interrupt vector table */
    .isr_vector :
    {
        . = ALIGN(256);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Program code */
    .text :
    {
        . = ALIGN(4);

        *(.text)
        *(.text*)

        *(.glue_7)
        *(.glue_7t)
        *(.vfp11_veneer)
        *(.v4_bx)

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Read-only data and constants */
    .rodata :
    {
        . = ALIGN(4);

        *(.rodata)
        *(.rodata*)

        . = ALIGN(4);
    } > FLASH

    /* ARM exception unwind tables */
    .ARM.extab :
    {
        . = ALIGN(4);

        *(.ARM.extab* .gnu.linkonce.armextab.*)

        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        . = ALIGN(4);

        __exidx_start = .;

        *(.ARM.exidx* .gnu.linkonce.armexidx.*)

        __exidx_end = .;

        . = ALIGN(4);
    } > FLASH

    /* Pre-initialization array */
    .preinit_array :
    {
        . = ALIGN(4);

        PROVIDE_HIDDEN(__preinit_array_start = .);

        KEEP(*(.preinit_array))
        KEEP(*(.preinit_array.*))

        PROVIDE_HIDDEN(__preinit_array_end = .);

        . = ALIGN(4);
    } > FLASH

    /* Initialization array */
    .init_array :
    {
        . = ALIGN(4);

        PROVIDE_HIDDEN(__init_array_start = .);

        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))

        PROVIDE_HIDDEN(__init_array_end = .);

        . = ALIGN(4);
    } > FLASH

    /* Finalization array */
    .fini_array :
    {
        . = ALIGN(4);

        PROVIDE_HIDDEN(__fini_array_start = .);

        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))

        PROVIDE_HIDDEN(__fini_array_end = .);

        . = ALIGN(4);
    } > FLASH

    /* Runtime initialization */
    .init :
    {
        . = ALIGN(4);

        KEEP(*(.init))

        . = ALIGN(4);
    } > FLASH

    /* Runtime finalization */
    .fini :
    {
        . = ALIGN(4);

        KEEP(*(.fini))

        . = ALIGN(4);
    } > FLASH

    /* Initialized global and static variables */
    .data :
    {
        . = ALIGN(4);

        _sdata = .;

        *(.data)
        *(.data*)

        . = ALIGN(4);

        _edata = .;

    } > RAM AT > FLASH

    /* Flash source address for .data initialization */
    _sidata = LOADADDR(.data);

    /* Zero-initialized global and static variables */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);

        _sbss = .;

        *(.bss)
        *(.bss*)

        *(COMMON)

        . = ALIGN(4);

        _ebss = .;

    } > RAM

    /* Heap and stack reservation */
    ._user_heap_stack (NOLOAD) :
    {
        . = ALIGN(8);

        _end = .;

        . += _Min_Heap_Size;
        . += _Min_Stack_Size;

        . = ALIGN(8);

    } > RAM

    /* Standard end symbol */
    PROVIDE(end = _end);

    /* Discard unnecessary metadata */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
    }
}

/* Link-time validation */
ASSERT(
    SIZEOF(.isr_vector) > 0,
    "Vector table is missing"
)

ASSERT(
    (_end + _Min_Heap_Size + _Min_Stack_Size) <= _estack,
    "RAM overflow: heap/stack reservation exceeds DTCM"
)