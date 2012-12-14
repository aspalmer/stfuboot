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

#include <libopencm3/stm32/crc.h>

#include <libopencm3/usb/usbd.h>

#include "dfu.h"
#include "uart.h"
#include "printf.h"

#define MIN(a, b) ((a)<(b) ? (a) : (b))

/* We need a special large control buffer for this device: */
u8 usbd_control_buffer[2048];

const struct usb_device_descriptor stfub_dev_descr = {
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
	.iManufacturer		= 0,
	.iProduct		= 1,
	.iSerialNumber		= 2,
	.bNumConfigurations	= 1,
};

const struct usb_dfu_descriptor stfub_dfu_descr = {
	.bLength		= sizeof(struct usb_dfu_descriptor),
	.bDescriptorType	= DFU_FUNCTIONAL,
	.bmAttributes		= USB_DFU_CAN_DOWNLOAD | USB_DFU_CAN_UPLOAD | USB_DFU_WILL_DETACH,
	.wDetachTimeout		= 255,
	.wTransferSize		= 2048, /* STM32 flash page size */
	.bcdDFUVersion		= 0x0110,
};

#define STFUB_AS_ISTRING(x) ((x) + 3)
#define STFUB_DFU_INTERFACE(num, func)				 \
	{							 \
		.bLength		= USB_DT_INTERFACE_SIZE, \
		.bDescriptorType	= USB_DT_INTERFACE,	 \
		.bInterfaceNumber	= 0,			 \
		.bAlternateSetting	= num,			 \
		.bNumEndpoints		= 0,			 \
		.bInterfaceClass	= 0xFE,			 \
		.bInterfaceSubClass	= 1,			 \
		.bInterfaceProtocol	= 2,			 \
		.iInterface		= STFUB_AS_ISTRING(num), \
		.extra			= &func,		 \
		.extralen		= sizeof(func),		 \
	}

const struct usb_interface_descriptor stfub_interface_descriptors[] ={
		STFUB_DFU_INTERFACE(STFUB_AS_MAIN_MEMORY, stfub_dfu_descr),
		STFUB_DFU_INTERFACE(STFUB_AS_SYSTEM_MEMORY, stfub_dfu_descr),
		STFUB_DFU_INTERFACE(STFUB_AS_OPTION_BYTES, stfub_dfu_descr),
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
	"Device with STFUBoot",
	serial_number_string,
	/* This string is used by ST Microelectronics' DfuSe utility. */
	"Main Memory [0x08004800 - 0x08040000]",
	"System Memory [0x08001000 - 0x08004800]",
	"Option Bytes [0x1FFFF800 - 0x1FFFF810]",
};

static void stfub_clocks_init(void)
{
	/*
	   TODO: For some reason the device would not be able to
	   initialize PLL after exiting the factory
	   bootloader (not due to flashing). Power-cycling the board will solve the issue.
	   Using HSE instead doesn't have this issuer
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

static usbd_device *stfub_usb_init(void)
{
	static usbd_device *usbddev;

	desig_get_unique_id_as_string(serial_number_string,
				      sizeof(serial_number_string));

	stfub_dfu_init(&stfub_dfu_descr);

	usbddev = usbd_init(&stm32f107_usb_driver, &stfub_dev_descr,
			    &config, usb_strings,
			    (sizeof(usb_strings) / sizeof(usb_strings[0])));
	usbd_set_control_buffer_size(usbddev, sizeof(usbd_control_buffer));

	usbd_register_set_altsetting_callback(usbddev,
					      stfub_dfu_switch_altsetting);
	usbd_register_control_callback(usbddev,
				       USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				       USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				       stfub_dfu_handle_control_request);

	return usbddev;
}

extern u32 stfub_firmware_crc(void);
extern u32 stfub_firmware_calc_crc(void);
extern u32 scratch_calc_crc(void);
extern u32 scratch_crc(void);
extern u32 stfub_info_calc_crc(void);
extern u32 stfub_info_crc(void);
extern u32 stfub_info_size(void);
int main(void)
{
	static usbd_device *usbddev;
	u32 crc, crc_calc;
	stfub_clocks_init();
	stfub_gpio_init();
	stfub_uart_init();

	rcc_peripheral_enable_clock(&RCC_AHBENR, RCC_AHBENR_CRCEN);
	/*
TODO: For some reason he first character of this banner is
lost, this has to be investigated further
*/
	stfub_printf("=========================================\n");
	stfub_printf("= stfuboot -- Insert smart tagline here =\n");
	stfub_printf("=========================================\n");

	usbddev = stfub_usb_init();

	while (1) {
		usbd_poll(usbddev);
		stfub_dfu_tick();
	}
}
