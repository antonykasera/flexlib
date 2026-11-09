/* linker.ld: the bench firmware, STM32F103 at 0x08000000 (ST-Link)
 * Needs 128K of flash: an F103CB, or a C8 that has it (check with st-info --probe). */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

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
    /* first, so it stays below the upper 32 KB that bench/cache_probe.c reads */
    *cache_probe.o(.text .text* .rodata .rodata*)
    _cache_probe_end = .;
    /* the bench harness: the cold flush reads its first 32 KB */
    EXCLUDE_FILE(*/utils/src/*.o */math/src/*.o *cmsis-dsp/*.o *libc_nano.a:*
                 *libc.a:* *libm.a:* *libgcc.a:*) *(.text .text* .rodata .rodata*)
    /* what cases time, so a cold sample finds none of it cached */
    . = ALIGN(32);
    _bench_measured = .;
    *(.bench_measured.text .bench_measured.rodata)
    *(.text .text* .rodata .rodata*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  ASSERT(_bench_measured >= ORIGIN(FLASH) + 0x8000,
         "bench harness under 32 KB: the cold flush would read measured code")

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

/* bench/cache_probe.c reads the upper 32 KB. Other code may live there, as
 * long as none of it runs during the probe: only the probe's own code does. */
ASSERT(_cache_probe_end <= ORIGIN(FLASH) + 32K,
       "cache probe code must stay below the upper 32 KB it reads")
