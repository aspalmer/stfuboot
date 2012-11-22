/*
 * This file is part of the stfuboot project.
 *
 * 	Copyright (C) 2012 Innovative Converged Devices (ICD)
 *
 * Author:
 *	Andrey Smirnov <andrey.smirnov@convergeddevices.net>
 *
 * Based on a simlar linker script form libopencm3 project
 *
 *	Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
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

/* Linker script for STM32F107VCT6, 256K flash, 64K RAM. */

INCLUDE stfub-mem-layout.ld

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
	} >ram AT >bl_rom
	_text_loadaddr = LOADADDR(.text);
	.data : {
		_data = .;
		*(.data*)	/* Read-write initialized data */
		. = ALIGN(4);
		_edata = .;
	} > ram AT > bl_rom
	_data_loadaddr = LOADADDR(.data);

	.reset_code : {
		    *(.text*)
	} > bl_rom

	.exception_handlers : {
		    *(.text*)
		    *(.rodata*)
	} > be_rom

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
