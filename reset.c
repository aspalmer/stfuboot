/*
 * This file is part of the stfuboot project.
 *
 * Copyright (C) 2012 Innovative Converged Devices (ICD)
 *
 * Author(s):
 *          Andrey Smirnov <andrey.smirnov@convergeddevices.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/f1/rcc.h>

#include <libstfub/scratch.h>
#include <libstfub/info_block.h>

#include "uart.h"

extern unsigned _data_loadaddr, _data, _edata, _ebss, _stack;
extern unsigned _text_loadaddr, _text, _etext;
extern unsigned _ram_start;

extern int main(void);



extern unsigned _if_rom_start;
extern unsigned _ap_rom_start;
extern unsigned _sy_rom_start;

static bool stfub_firmware_is_valid(void)
{
	u32 crc;
	struct stfub_firmware_info *info_block;

	info_block = (struct stfub_firmware_info *)&_if_rom_start;

#if 0
	crc = crc_calculate_block((u32 *) info_block,
				  sizeof(*info_block) / 4 - 1);

	if (crc != info_block->crc.info_block)
		return false;
#endif
	crc = crc_calculate_block((u32 *)&_ap_rom_start,
				  info_block->size / 4);

	crc = 0xDEADBEEF;

	/* return crc == info_block->crc.firmware; */

	return true;
}


__attribute__ ((noreturn, interrupt))
void stfub_ram_reset_handler(void)
{
	volatile unsigned *src, *dest;

 	asm volatile("msr msp, %0" : : "r"(&_stack));

	/* Copy .data section to system RAM */
	for (src = &_data_loadaddr, dest = &_data; dest < &_edata; src++, dest++)
		*dest = *src;

	while (dest < &_ebss)
		*dest++ = 0;

	/* Call the application's entry point. */
	main();

	for(;;)
		asm volatile("nop");
}


void null_handler(void);

static void stfub_reset_start_clocks(void)
{
	rcc_clock_setup_in_hsi_out_48mhz();
	rcc_peripheral_enable_clock(&RCC_AHBENR, RCC_AHBENR_CRCEN);
}

static void stfub_reset_stop_clocks(void)
{
	rcc_peripheral_disable_clock(&RCC_AHBENR, RCC_AHBENR_CRCEN);
	rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_HSICLK);
	rcc_osc_off(PLL);
}


#define STFUB_PANIC()							\
	do {								\
		register vector_table_t *vtable;			\
		register struct scb_exception_stack_frame *frame;	\
									\
		SCB_GET_EXCEPTION_STACK_FRAME(frame);			\
									\
		vtable		= (vector_table_t *)&_sy_rom_start;	\
		SCB_VTOR	= (u32)vtable;				\
		frame->pc	= (u32)vtable->reset;			\
									\
		asm volatile ("bx lr");					\
	} while (0)


__attribute__ ((naked, interrupt))
void stfub_ram_retreat_to_factory_dfu(void)
{
	STFUB_PANIC();
}

__attribute__ ((section(".reset_code"), naked, noreturn, interrupt))
void stfub_rom_reset_handler(void)
{
	bool scratchpad_is_valid, firmware_is_valid;

	volatile unsigned *src, *dest;
	vector_table_t *vtable;

	/* Copy the .text section to the the system RAM */
	for (src = &_text_loadaddr, dest = &_text; dest < &_etext; src++, dest++)
		*dest = *src;

	stfub_reset_start_clocks();
	scratchpad_is_valid	= stfub_scratchpad_is_valid();
	firmware_is_valid	= false/* stfub_firmware_is_valid() */;
	stfub_reset_stop_clocks();

	if ((scratchpad_is_valid && stfub_scratchpad_dfu_switch_requested())
	    || !firmware_is_valid) {
		/* Patch vector table so it would point to correct handlers
		 * located in RAM */
		vtable = (vector_table_t *)&_ram_start;

		vtable->reset	= stfub_ram_reset_handler;
		SCB_VTOR	= (u32)vtable;
		vtable->reset();
	} else {
		vtable = (vector_table_t *)&_ap_rom_start;

		SCB_VTOR = (u32)vtable;
		vtable->reset();
	}
}

__attribute__ ((section(".exception_handlers"), naked, interrupt))
void stfub_rom_retreat_to_factory_dfu(void)
{
	STFUB_PANIC();
}

__attribute__ ((section(".exception_handlers"), interrupt))
void stfub_rom_null_handler(void)
{
	/* Do nothing. */
}

void reset_handler(void)		__attribute__ ((alias ("stfub_rom_reset_handler")));
void nmi_handler(void)			__attribute__ ((alias ("stfub_rom_null_handler")));
void hard_fault_handler(void)		__attribute__ ((alias ("stfub_rom_retreat_to_factory_dfu")));
void mem_manage_handler(void)		__attribute__ ((alias ("stfub_rom_retreat_to_factory_dfu")));
void bus_fault_handler(void)		__attribute__ ((alias ("stfub_rom_retreat_to_factory_dfu")));
void usage_fault_handler(void)		__attribute__ ((alias ("stfub_rom_retreat_to_factory_dfu")));
void sv_call_handler(void)		__attribute__ ((alias ("stfub_rom_null_handler")));
void debug_monitor_handler(void)	__attribute__ ((alias ("stfub_rom_null_handler")));
void pend_sv_handler(void)		__attribute__ ((alias ("stfub_rom_null_handler")));
void sys_tick_handler(void)		__attribute__ ((alias ("stfub_rom_null_handler")));
