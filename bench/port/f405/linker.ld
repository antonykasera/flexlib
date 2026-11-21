/* STM32F405RG; the CCM RAM is not used */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x800;

ENTRY(Reset_Handler)

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    EXCLUDE_FILE(*/utils/src/*.o */math/src/*.o */dsp/src/*.o */nn/src/*.o *cmsis-dsp/*.o
                 *cmsis-nn/*.o *libc_nano.a:*
                 *libc.a:* *libm.a:* *libgcc.a:*) *(.text .text* .rodata .rodata*)
    /* timed code on a 4 KB step */
    . = ALIGN(4096);
    _bench_measured = .;
    *(.bench_measured.text .bench_measured.rodata)
    *(.text .text* .rodata .rodata*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .ARM.exidx :
  {
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
  } > FLASH

  .init_array :
  {
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } > FLASH

  .fini_array :
  {
    . = ALIGN(4);
    __fini_array_start = .;
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array))
    __fini_array_end = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
