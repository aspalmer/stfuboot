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

/* Define memory regions. */
MEMORY
{
	ram	(rwx)	: ORIGIN = 0x20000000, LENGTH = 65504 /* 64K - 32*/
	scratch	(rw)	: ORIGIN = 0x2000FFE0, LENGTH = 32
	bl_rom	(rx)	: ORIGIN = 0x08000000, LENGTH = 16K
	be_rom	(rx)	: ORIGIN = 0x08004000, LENGTH = 2K
	if_rom	(rx)	: ORIGIN = 0x08004800, LENGTH = 512
	ap_rom	(rx)	: ORIGIN = 0x08004A00, LENGTH = 243200 /* 256K - 18K - 512*/
	sy_rom	(rx)	: ORIGIN = 0x1FFFB000, LENGTH = 18K
	op_rom	(r)	: ORIGIN = 0x1FFFF800, LENGTH = 16
}

PROVIDE(_ram_start	= ORIGIN(ram));
PROVIDE(_ram_end	= ORIGIN(ram) + LENGTH(ram));
PROVIDE(_stack		= _ram_end);
PROVIDE(_scratch	= ORIGIN(scratch));
PROVIDE(_bl_rom_start	= ORIGIN(bl_rom));
PROVIDE(_bl_rom_end	= ORIGIN(bl_rom) + LENGTH(bl_rom));
PROVIDE(_if_rom_start	= ORIGIN(if_rom));
PROVIDE(_if_rom_end	= ORIGIN(if_rom) + LENGTH(if_rom));
PROVIDE(_ap_rom_start	= ORIGIN(ap_rom));
PROVIDE(_ap_rom_end	= ORIGIN(ap_rom) + LENGTH(ap_rom));
PROVIDE(_sy_rom_start	= ORIGIN(sy_rom));
