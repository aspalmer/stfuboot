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

#include "uart.h"

extern unsigned _data_loadaddr, _data, _edata, _ebss, _stack;
extern unsigned _text_loadaddr, _text, _etext;
extern unsigned _ram_start;

extern int main(void);

__attribute__ ((noreturn))
void ram_reset_handler(void)
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

extern volatile struct uart_buffer uart_tx;
void usart2_isr(void)
{
	char c;
	if ((USART_SR(USART2) & USART_SR_TXE)  != 0) {
		if (uart_buffer_pop(&uart_tx, &c) == 0) {
			usart_send(USART2, (uint16_t)c);
		} else {
			usart_disable_tx_interrupt(USART2);
		}
	}
}


void blocking_handler(void);
void null_handler(void);

__attribute__ ((section(".reset_code"), naked, noreturn))
void reset_handler(void)
{
	volatile unsigned *src, *dest;
	vector_table_t *vtable;

#if 0				/* TODO: implement jumping to DFU
				 * from user application*/
	if (get_pc() > (u32) &_system_rom_start)
		/* Application jumped to DFU */
		src = (unisgned *)((u32) &_system_rom_start + (u32) &_text_loadaddr);
	else
		src = &_text_loadaddr;
#endif


	/* Copy the .text section to the the system RAM */
	for (src = &_text_loadaddr, dest = &_text; dest < &_etext; src++, dest++)
		*dest = *src;

	/* Patch vector table so it would point to correct handlers
	 * located in RAM */
	vtable = (vector_table_t *) &_ram_start;

	vtable->reset			= ram_reset_handler;
	vtable->nmi			= null_handler;
	vtable->hard_fault		= blocking_handler;
	vtable->memory_manage_fault	= blocking_handler;
	vtable->bus_fault		= blocking_handler;
	vtable->usage_fault		= blocking_handler;
	vtable->sv_call			= null_handler;
	vtable->debug_monitor		= null_handler;
	vtable->pend_sv			= null_handler;
	vtable->systick			= null_handler;

 	SCB_VTOR = (u32)vtable;
	vtable->reset();
}

__attribute__ ((section(".reset_code"), naked))
void rom_blocking_handler(void)
{
	while (1) ;
}

__attribute__ ((section(".reset_code"), naked))
void rom_null_handler(void)
{
	/* Do nothing. */
}

void nmi_handler(void)			__attribute__ ((alias ("rom_null_handler")));
void hard_fault_handler(void)		__attribute__ ((alias ("rom_blocking_handler")));
void mem_manage_handler(void)		__attribute__ ((alias ("rom_blocking_handler")));
void bus_fault_handler(void)		__attribute__ ((alias ("rom_blocking_handler")));
void usage_fault_handler(void)		__attribute__ ((alias ("rom_blocking_handler")));
void sv_call_handler(void)		__attribute__ ((alias ("rom_null_handler")));
void debug_monitor_handler(void)	__attribute__ ((alias ("rom_null_handler")));
void pend_sv_handler(void)		__attribute__ ((alias ("rom_null_handler")));
void sys_tick_handler(void)		__attribute__ ((alias ("rom_null_handler")));
