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

#ifndef _UART_H_
#define _UART_H_

#define UART_BUFFER_SIZE 256

struct uart_buffer{
	unsigned char start;
	unsigned char end;
	unsigned char watermark;

	char data[UART_BUFFER_SIZE];
};

void stfub_uart_init(void);
int uart_buffer_push(volatile struct uart_buffer *buf, char c);
int uart_buffer_pop(volatile struct uart_buffer *buf, char *c);
void uart_putchar(char c);

#endif /* _UART_H_ */
