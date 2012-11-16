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

struct dfu_device;

struct dfu_device_ops {
	int (*read_block) (struct dfu_device *dfu, u16 block_no,
			   u8 *buf, int len, void *context);
	int (*write_block) (struct dfu_device *dfu, u16 block_no,
			    const u8 * buf, int len, void *context);
	bool (*all_data_in) (struct dfu_device *dfu, void *context);
};


struct dfu_device {
	const struct usb_dfu_descriptor *descr;
	const struct dfu_device_ops     *ops;

	void *context;
	u32 timeout;

	enum dfu_status status;
	enum dfu_state  state;

	struct {
		int num;
		u8 *buf;
		int buf_size;
		int len;
	} block;


	bool block_completed;
};

int dfu_device_handle_control_request(usbd_device *udbddev, struct usb_setup_data *req, u8 **buf,
				      u16 *len, void (**complete)(usbd_device *udbddev, struct usb_setup_data *req));

void dfu_device_tick(struct dfu_device *dfu);

void dfu_device_init(struct dfu_device *dfu,
		     const struct usb_dfu_descriptor *descr,
		     const struct dfu_device_ops     *ops,
		     u8 *buf, int buf_size,
		     void *context);

struct dfu_device *dfu_device_get_dfu_device(void);

void stfub_dfu_swith_altsetting(usbd_device *usbd_dev, u16 interface, u16 altsetting);

#endif
