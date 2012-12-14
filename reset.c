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


/**
 *  ---- 0x08000000 ----
 *   permanent vector table
 *   permanent vector handlers' code
 *  ---- 0x08001000 ----
 *   reset vector handler
 *   main vector table
 *   bootloader code
 *  ---- 0x08004800 ----
 *   fw information block
 *  ---- 0x08004a00 ----
 *   application code
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

	crc_reset();
	crc = crc_calculate_block((u32 *) info_block,
			(sizeof(*info_block) / 4) - 1);

	if (crc != info_block->crc.info_block)
		return false;

	crc_reset();
	crc = crc_calculate_block((u32 *)&_ap_rom_start,
			info_block->size / 4-1);

	return crc == info_block->crc.firmware;
	/* force to dfu mode if previous line commented */
	return false;
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

	__attribute__ ((section(".exception_handlers"), naked))
static void stfub_start_with_vector_table_at_offset(void *table)
{
	vector_table_t *vtable = table;
	SCB_VTOR = (u32)vtable;
	vtable->reset();
}

volatile bool scratchpad_is_valid, firmware_is_valid;
	__attribute__ ((section(".reset_code"), naked, noreturn, interrupt))
void stfub_rom_reset_handler(void)
{

	volatile unsigned *src, *dest;
	vector_table_t *vtable;

	/* Copy the interrupt vector table to the the system RAM */
	for (src = &_text_loadaddr, dest = &_text; dest < &_etext; src++, dest++)
		*dest = *src;

	/* Copy the .text section to the the system RAM */
	for (src = &_text_loadaddr, dest = &_text; dest < &_etext; src++, dest++)
		*dest = *src;

	stfub_reset_start_clocks();
	scratchpad_is_valid	= stfub_scratchpad_is_valid();
	firmware_is_valid	= stfub_firmware_is_valid();
	stfub_reset_stop_clocks();

	if ((scratchpad_is_valid && stfub_scratchpad_dfu_switch_requested())
			|| !firmware_is_valid) {
		/* Patch vector table so it would point to correct handlers
		 * located in RAM */
		vtable = (vector_table_t *)&_ram_start;
		vtable->reset	= stfub_ram_reset_handler;

		stfub_start_with_vector_table_at_offset(&_ram_start);
	} else {
		stfub_start_with_vector_table_at_offset(&_ap_rom_start);
	}
}

	__attribute__ ((section(".exception_handlers")))
bool stfub_exception_handlers_pages_are_protected(void)
{
	extern unsigned _op_rom_start;
	struct option_bytes *obytes = (struct option_bytes *) &_op_rom_start;

	/* FIXME: checking for 0 instead of 1 */
#if 0
	return (obytes->wrp0 & (FLASH_WRP_PAGE(0) | FLASH_WRP_PAGE(1))) ==
		(FLASH_WRP_PAGE(0) | FLASH_WRP_PAGE(1));
#else
	return true;
#endif
}

	__attribute__ ((section(".exception_handlers")))
bool stfub_bootloader_update_failed(void)
{
	extern unsigned _op_rom_start;
	struct option_bytes *obytes = (struct option_bytes *) &_op_rom_start;

#if 0
	return (obytes->data0 == ~obytes->ndata0) &&
		(obytes->data1 == ~obytes->ndata1) &&
		obytes->data0 == 0x42		  &&
		obytes->data1 == 0x24;
#else
	return false;
#endif

}

	__attribute__ ((section(".exception_handlers"), naked, interrupt))
void stfub_rom_early_reset(void)
{
	if (!stfub_exception_handlers_pages_are_protected() ||
			stfub_bootloader_update_failed())
		stfub_start_with_vector_table_at_offset(&_sy_rom_start);
	else
		stfub_start_with_vector_table_at_offset(&_text_loadaddr);
}

	__attribute__ ((section(".exception_handlers"), naked, interrupt))
void stfub_rom_retreat_to_factory_dfu(void)
{
	register vector_table_t *vtable;
	register struct scb_exception_stack_frame *frame;

	SCB_GET_EXCEPTION_STACK_FRAME(frame);

	vtable		= (vector_table_t *)&_sy_rom_start;
	SCB_VTOR	= (u32)vtable;
	frame->pc	= (u32)vtable->reset;

	asm volatile ("bx lr");
}

	__attribute__ ((section(".exception_handlers"), naked, interrupt))
void stfub_rom_null_handler(void)
{
	/* Do nothing. */
	asm volatile ("bx lr");
}

__attribute__ ((section(".vectors")))
vector_table_t pvector_table = {
	.initial_sp_value	= &_stack,
	.reset			= stfub_rom_early_reset,
	.nmi			= stfub_rom_null_handler,
	.hard_fault		= stfub_rom_retreat_to_factory_dfu,
	.memory_manage_fault	= stfub_rom_retreat_to_factory_dfu,
	.bus_fault		= stfub_rom_retreat_to_factory_dfu,
	.usage_fault		= stfub_rom_retreat_to_factory_dfu,
	.debug_monitor		= stfub_rom_null_handler,
	.sv_call		= stfub_rom_null_handler,
	.pend_sv		= stfub_rom_null_handler,
	.systick		= stfub_rom_null_handler,
	.irq			= {
		[0 ... NVIC_IRQ_COUNT - 1] = stfub_rom_null_handler
	}
};


#define ALIAS(name) __attribute__((alias (#name)))

void reset_handler(void)		ALIAS(stfub_rom_reset_handler);
void nmi_handler(void)			ALIAS(stfub_rom_null_handler);
void hard_fault_handler(void)		ALIAS(stfub_rom_retreat_to_factory_dfu);
void mem_manage_handler(void)		ALIAS(stfub_rom_retreat_to_factory_dfu);
void bus_fault_handler(void)		ALIAS(stfub_rom_retreat_to_factory_dfu);
void usage_fault_handler(void)		ALIAS(stfub_rom_retreat_to_factory_dfu);
void sv_call_handler(void)		ALIAS(stfub_rom_null_handler);
void debug_monitor_handler(void)	ALIAS(stfub_rom_null_handler);
void pend_sv_handler(void)		ALIAS(stfub_rom_null_handler);
void sys_tick_handler(void)		ALIAS(stfub_rom_null_handler);
