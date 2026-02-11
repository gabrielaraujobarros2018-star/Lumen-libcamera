/* libcamera.ld - Linker script for Nexus 6 (Shamu) camera library */
/* ARMv7-A Snapdragon 805 memory layout - Shared library optimized */

MEMORY
{
  FLASH   (rx) : ORIGIN = 0xB0000000, LENGTH = 64M   /* Nexus 6 /system/lib */
  RAM     (rwx) : ORIGIN = 0x40000000, LENGTH = 128M  /* Application RAM */
  STACK   (rwx) : ORIGIN = 0x7FF00000, LENGTH = 1M    /* Thread stacks */
  HEAP    (rwx) : ORIGIN = 0x78000000, LENGTH = 64M   /* Camera frame buffers */
}

SECTIONS
{
  /* Start at Nexus 6 shared library base */
  . = 0xB0000000;

  /* 4KB alignment for ARMv7/DMA */
  . = ALIGN(4096);

  /* Code section - NEON optimized, read-execute */
  .text : {
    KEEP(*(.init))
    *(.text .text.*)
    *(.gnu.linkonce.t.*)
    . = ALIGN(16);  /* NEON vector alignment */
  } > FLASH

  /* Read-only data - camera calibration tables */
  .rodata : {
    *(.rodata .rodata.*)
    *(.gnu.linkonce.r.*)
    . = ALIGN(4096);
  } > FLASH

  /* Initial data values (loaded to RAM at runtime) */
  .data : {
    _data_start = .;
    *(.data .data.*)
    *(.gnu.linkonce.d.*)
    _data_end = .;
  } > RAM AT > FLASH

  /* BSS - Zero-initialized data (camera state) */
  .bss : {
    _bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    _bss_end = .;
  } > RAM

  /* Camera frame buffers - 4K page aligned */
  .framebuffers 0x78000000 : {
    _fb_start = .;
    . = ALIGN(4096);
    *(.framebuffers)
    . = ALIGN(4096);
    _fb_end = .;
  } > HEAP

  /* Stack for camera worker threads */
  _stack_top = 0x7FF00000 + 1M;

  /* Debug sections (stripped in release) */
  .debug_* : { *(.debug_*) } > RAM

  /* Discard unused sections */
  /DISCARD/ : {
    *(.comment)
    *(.note.gnu.build-id)
  }
}

/* ARMv7 ABI symbols for dynamic linking */
PROVIDE(__aeabi_uidiv = __aeabi_uidivmod);
PROVIDE(__aeabi_idiv = __aeabi_idivmod);

/* NEON FPU configuration */
PHDRS
{
  text PT_LOAD FLAGS(5);          /* R E */
  data PT_LOAD FLAGS(6);          /* RW */
}