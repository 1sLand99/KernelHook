/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linker script for freestanding .ko build (Approach B).
 *
 * A .ko is an ET_REL (relocatable) ELF. We use ld -r (partial link)
 * so this script only orders sections — all relocations are preserved.
 */

SECTIONS
{
    .text : {
        *(.text .text.*)
    }

    .plt : {
        *(.plt)
    }

    .init.plt : {
        *(.init.plt)
    }

    .init.text : {
        *(.init.text)
    }

    .exit.text : {
        *(.exit.text)
    }

    .transit.data : {
        *(.transit.data)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .modinfo : {
        *(.modinfo)
    }

    __versions : {
        *(__versions)
    }

    .gnu.linkonce.this_module : {
        *(.kh.this_module)
    }

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
    }

    .kp.symbol : {
        *(.kp.symbol)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
    }
}
