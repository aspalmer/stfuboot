/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Linker script for Olimex STM32-H107 (STM32F107VCT6, 256K flash, 64K RAM). */

/* Define memory regions. */
MEMORY
{
	boot_mem (rx)	 : ORIGIN = 0x00000000, LENGTH = 18K,
	ram (rwx)	 : ORIGIN = 0x20000000, LENGTH = 64K
	rom (rx)	 : ORIGIN = 0x08000000, LENGTH = 256K
	system_rom (rx)	 : ORIGIN = 0x1FFFB000, LENGTH = 18K
	option_bytes (r) : ORIGIN = 0x1FFFF800, LENGTH = 16
}

PROVIDE(_stack = ORIGIN(ram) + LENGTH(ram));
PROVIDE(_ram_start = ORIGIN(ram));
PROVIDE(_system_rom_start = ORIGIN(system_rom));
PROVIDE(_system_rom_end	= ORIGIN(system_rom) + LENGTH(system_rom));
PROVIDE(_app_rom_start = ORIGIN(rom));
PROVIDE(_app_rom_end = ORIGIN(rom) + LENGTH(rom));
PROVIDE(_option_bytes_start = ORIGIN(option_bytes));
PROVIDE(_option_bytes_end = ORIGIN(option_bytes) + LENGTH(option_bytes));

/* Enforce emmition of the vector table. */
EXTERN (vector_table)

/* Define the entry point of the output file. */
ENTRY(reset_handler)

/* Define sections. */
SECTIONS
{
	.text : {
		_text = .;
		*(.vectors)	/* Vector table */
		*(.text*)	/* Program code */
		. = ALIGN(4);
		*(.rodata*)	/* Read-only data */
		. = ALIGN(4);
		_etext = .;
	} >ram AT >boot_mem
	_text_loadaddr = LOADADDR(.text);

	.reset_code : {
		    . = ALIGN(4);
		    *(.text.reset_handler)
		    *(.text.sv_call_handler)
	} >boot_mem

	.data : {
		_data = .;
		*(.data*)	/* Read-write initialized data */
		. = ALIGN(4);
		_edata = .;
	} >ram AT >boot_mem
	_data_loadaddr = LOADADDR(.data);

	.bss : {
		*(.bss*)	/* Read-write zero initialized data */
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} >ram

	/*
	 * The .eh_frame section appears to be used for C++ exception handling.
	 * You may need to fix this if you're using C++.
	 */
	/DISCARD/ : { *(.eh_frame) }

	. = ALIGN(4);
	end = .;
}
