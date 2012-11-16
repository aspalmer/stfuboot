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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

#include <libopencm3/cm3/scb.h>

#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/flash.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/flash.h>
#include <libopencm3/stm32/desig.h>


#include <libopencm3/usb/usbd.h>

#include "dfu.h"
#include "uart.h"
#include "printf.h"

#define MIN(a, b) ((a)<(b) ? (a) : (b))

static usbd_device *usbddev = NULL;
static struct dfu_device *dfu = NULL;
/* We need a special large control buffer for this device: */
u8 usbd_control_buffer[2048];
u8 dfu_data_buffer[2048];


const struct usb_device_descriptor dev = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= 0x0200,
	.bDeviceClass		= 0,
	.bDeviceSubClass	= 0,
	.bDeviceProtocol	= 0,
	.bMaxPacketSize0	= 64,
	.idVendor		= 0x0483,
	.idProduct		= 0xDF11,
	.bcdDevice		= 0x0200,
	.iManufacturer		= 1,
	.iProduct		= 2,
	.iSerialNumber		= 3,
	.bNumConfigurations	= 1,
};

const struct usb_dfu_descriptor dfu_function = {
	.bLength		= sizeof(struct usb_dfu_descriptor),
	.bDescriptorType	= DFU_FUNCTIONAL,
	.bmAttributes		= USB_DFU_CAN_UPLOAD | USB_DFU_WILL_DETACH,
	.wDetachTimeout		= 255,
	.wTransferSize		= 2048, /* STM32 flash page size */
	.bcdDFUVersion		= 0x0110,
};

const struct usb_interface_descriptor stfub_interface_descriptors[] ={
	{
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0,
		.bAlternateSetting	= 0,
		.bNumEndpoints		= 0,
		.bInterfaceClass	= 0xFE, /* Device Firmware Upgrade */
		.bInterfaceSubClass	= 1,
		.bInterfaceProtocol	= 2,
		.iInterface		= 4,
		.extra			= &dfu_function,
		.extralen		= sizeof(dfu_function),
	},
	{
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0,
		.bAlternateSetting	= 1,
		.bNumEndpoints		= 0,
		.bInterfaceClass	= 0xFE, /* Device Firmware Upgrade */
		.bInterfaceSubClass	= 1,
		.bInterfaceProtocol	= 2,
		.iInterface		= 5,
		.extra			= &dfu_function,
		.extralen		= sizeof(dfu_function),
	},
	{
		.bLength		= USB_DT_INTERFACE_SIZE,
		.bDescriptorType	= USB_DT_INTERFACE,
		.bInterfaceNumber	= 0,
		.bAlternateSetting	= 2,
		.bNumEndpoints		= 0,
		.bInterfaceClass	= 0xFE, /* Device Firmware Upgrade */
		.bInterfaceSubClass	= 1,
		.bInterfaceProtocol	= 2,
		.iInterface		= 6,
		.extra			= &dfu_function,
		.extralen		= sizeof(dfu_function),
	},
};

struct usb_interface stfub_interfaces[] = {
	{
		.num_altsetting = 3,
		.altsetting	= stfub_interface_descriptors,
	},
};

struct usb_config_descriptor config = {
	.bLength		= USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType	= USB_DT_CONFIGURATION,
	.wTotalLength		= 0,
	.bNumInterfaces		= 1,
	.bConfigurationValue	= 1,
	.iConfiguration		= 0,
	.bmAttributes		= 0xC0,
	.bMaxPower		= 0x32,

	.interface		= stfub_interfaces,
};

static char serial_number_string[30];
static const char *usb_strings[] = {
	"Innovative Converged Devices",
	"Device with STFUBoot",
	serial_number_string,
	/* This string is used by ST Microelectronics' DfuSe utility. */
	"Main Memory [0x08000000 - 0x0803FFFF]",
	"System Memory [0x1FFFB000 - 0x1FFFF7FF]",
	"Option Bytes [0x1FFFF800 - 0x1FFFF80F]",
};

static int usbdfu_read_block(struct dfu_device *dfu, u16 block_no,
			     u8 *buf, int len, void *context)
{
	int read_len;
	const u8 *start_address  = (const u8 *)0x1FFFB000;
	const u8 *end_address    = (const u8 *)0x1FFFF800;

	static const u8 *block;

	if (block_no == 0)
		block = start_address;

	read_len = MIN(len, end_address - block);

	memcpy(buf, block, read_len);

	block += read_len;

	return read_len;
}

static int usbdfu_write_block(struct dfu_device *dfu, u16 block_no,
			      const u8 *buf, int len, void *context)
{
	/* 
	   FIXME: I feel like this code is not handling all the edge
	   cases. Someday it will have to be improved.
	 */

#if 0
	int write_len, i;
	const u8 *start_address  = (const u8 *)0x08000000;
	const u8 *end_address    = (const u8 *)0x08040000;

	static const u8 *block;

	if (block_no == 0)
		block = start_address;

	write_len = MIN(len, end_address - block);

	flash_unlock();

	/* Clear the least significat bit to get the highest even
	 * number less or eqal to write_len */
	for (i = 0; i < (write_len & (~0x1)); i += 2)
		flash_program_half_word((u32)(block + i), *(u16 *)(buf + i));

	if (i != write_len) {
		/* It looks like len is odd and therefor the last byte
		 * was not written do it here*/
		/* 
		   FIXME: Should it be
		   u16 word = buf[len - 1] << 8;
		   ?
		 */
		u16 word = buf[write_len - 1];
		flash_program_half_word((u32)(block + i), word);
	}

	flash_lock();


	block += write_len;
#endif
	return 0;
}

static bool usbdfu_all_data_in(struct dfu_device *dfu, void *context)
{
	return true;
}

static const struct dfu_device_ops usbdfu_dfu_ops = {
	.read_block	= usbdfu_read_block,
	.write_block	= usbdfu_write_block,
	.all_data_in    = usbdfu_all_data_in,
};

static void stfub_clocks_init(void)
{
	/*
	   TODO: For some reason the deivce would not be able to
	   initialize PLL after if this FW is flashed using factory
	   bootloader. Power-cycling the board will sole the issue.
	 */
	rcc_clock_setup_in_hsi_out_48mhz();
	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_AFIOEN);

	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPDEN);
	rcc_peripheral_enable_clock(&RCC_APB1ENR, RCC_APB1ENR_USART2EN);

	rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_IOPAEN);
	rcc_peripheral_enable_clock(&RCC_AHBENR,  RCC_AHBENR_OTGFSEN);
}

static void stfub_gpio_init(void)
{
	gpio_set_mode(GPIOD, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO5);
	gpio_set(GPIOD, GPIO5);

	gpio_primary_remap(AFIO_MAPR_SWJ_CFG_FULL_SWJ, AFIO_MAPR_USART2_REMAP);
}

static void stfub_usb_init(void)
{
	desig_get_unique_id_as_string(serial_number_string,
				      sizeof(serial_number_string));

	dfu = dfu_device_get_dfu_device();
	dfu_device_init(dfu, &dfu_function, &usbdfu_dfu_ops,
			dfu_data_buffer, dfu_function.wTransferSize, NULL);


	usbddev = usbd_init(&stm32f107_usb_driver, &dev, &config, usb_strings,
			    (sizeof(usb_strings) / sizeof(usb_strings[0])));
	usbd_set_control_buffer_size(usbddev, sizeof(usbd_control_buffer));

	usbd_register_set_altsetting_callback(usbddev, stfub_dfu_swith_altsetting);
	usbd_register_control_callback(usbddev,
				       USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				       USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				       dfu_device_handle_control_request);
}

int main(void)
{
	stfub_clocks_init();
	stfub_gpio_init();
	stfub_uart_init();
	
	/* 
	   TODO: For some reason the first character of this banner is
	   lost, this has to be investigated further
	 */
	stfub_printf("=========================================\n");
	stfub_printf("= stfuboot -- Insert smart tagline here =\n");
	stfub_printf("=========================================\n");

	stfub_usb_init();
	
	while (1) {
		usbd_poll(usbddev);
		dfu_device_tick(dfu);
	}
}
