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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/f1/gpio.h>

#include "uart.h"

#define UART_BUF_COUNT(b)        \
        (b->end >= b->start) ? (b->end - b->start) : \
        (UART_BUFFER_SIZE - b->start + b->end)

int uart_buffer_push(volatile struct uart_buffer *buf, char c)
{
	unsigned char count;
	int ret = 0;

	if (buf != NULL) {
		if ((buf->end + 1) % UART_BUFFER_SIZE != buf->start) {
			buf->data[buf->end] = c;
			buf->end = (buf->end + 1) % UART_BUFFER_SIZE;
			count = UART_BUF_COUNT(buf);
			if (count > buf->watermark) {
				buf->watermark = count;
			}
		} else {
			ret = -ENOMEM;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

int uart_buffer_pop(volatile struct uart_buffer *buf, char *c)
{
	int ret = 0;
	if (buf != NULL && c != NULL) {
		if (buf->end != buf->start) {
			*c = buf->data[buf->start];
			buf->start = (buf->start + 1) % UART_BUFFER_SIZE;
		} else {
			ret = -EBUSY;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static volatile struct uart_buffer uart_tx;

void uart_putchar(char c)
{
	int n = 0;

	if (c == '\n')
		uart_putchar('\r');
	
	if (usart_tx_interrupt_enabled(USART2)) {
		while (uart_buffer_push(&uart_tx, c) < 0 && ++n < 1000);
	} else {
		usart_enable_tx_interrupt(USART2);
		usart_send(USART2, (uint16_t)c);
	}
}

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

void stfub_uart_init(void)
{
	memset((void *)&uart_tx,0,sizeof(uart_tx));

	nvic_enable_irq(NVIC_USART2_IRQ);
	nvic_set_priority(NVIC_USART2_IRQ, 3);

	usart_set_baudrate(USART2, 115200);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_CR2_STOPBITS_1);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
	usart_set_mode(USART2, USART_MODE_TX);
	usart_enable(USART2);
	usart_disable_tx_interrupt(USART2);
}
