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

#ifndef __DFU_DEVICE_H__
#define __DFU_DEVICE_H__

#include <libopencm3/usb/dfu.h>
#include <libopencm3/usb/usbd.h>

#include "printf.h"

enum stfub_memory_region_altsetting {
	STFUB_AS_MAIN_MEMORY = 0,
	STFUB_AS_SYSTEM_MEMORY,
	STFUB_AS_OPTION_BYTES,
};

int stfub_dfu_handle_control_request(usbd_device *udbddev,
				     struct usb_setup_data *req,
				     u8 **buf, u16 *len,
				     void (**complete)(usbd_device *usbddev,
						       struct usb_setup_data *req));
void stfub_dfu_tick(void);
void stfub_dfu_init(const struct usb_dfu_descriptor *descr);
void stfub_dfu_switch_altsetting(usbd_device *usbd_dev, u16 interface, u16 altsetting);

#endif
