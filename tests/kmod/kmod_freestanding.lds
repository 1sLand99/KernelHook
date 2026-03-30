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
        *(.init.text)
        *(.exit.text)
        *(.text .text.*)
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
