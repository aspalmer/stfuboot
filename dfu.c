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
#include <stdbool.h>

#include <libopencm3/usb/usbd.h>

#include "dfu.h"

#define MIN(a, b) ((a)<(b) ? (a) : (b))

struct stfub_memory_bank {
	u32 start, end;
};

static const struct stfub_memory_bank stfub_memory_banks[] = {
	[STFUB_AS_MAIN_MEMORY] = {
		.start	= 0x08000000,
		.end	= 0x08040000,
	},
	[STFUB_AS_SYSTEM_MEMORY] = {
		.start	= 0x1FFFB000,
		.end	= 0x1FFFF800,
	},
	[STFUB_AS_OPTION_BYTES] = {
		.start	= 0x1FFFF800,
		.end	= 0x1FFFF810,
	},
};

struct stfub_dfu {
	const struct usb_dfu_descriptor *descr;

	u32 timeout;

	enum dfu_status status;
	enum dfu_state  state;

	struct {
		const u8 *readptr;
		u8 *writeptr;
	} block;

	const struct stfub_memory_bank *bank;
};

static struct stfub_dfu dfu;

void stfub_dfu_init(const struct usb_dfu_descriptor *descr)
{
	dfu.state	= STATE_DFU_IDLE;
	dfu.status	= DFU_STATUS_OK;
	dfu.descr	= descr;
	dfu.timeout	= 100;
	dfu.bank	= &stfub_memory_banks[STFUB_AS_MAIN_MEMORY];
}

void stfub_dfu_switch_altsetting(usbd_device *usbd_dev, u16 interface, u16 altsetting)
{
	dfu.bank = &stfub_memory_banks[altsetting];
}

static int stfub_dfu_read_firmware_block(struct stfub_dfu *dfu, u16 block_no,
					 u8 *buf, int len)
{
	int read_len;
	const u8 *start_address  = (const u8 *)dfu->bank->start;
	const u8 *end_address    = (const u8 *)dfu->bank->end;

	if (block_no == 0)
		dfu->block.readptr = start_address;

	read_len = MIN(len, end_address - dfu->block.readptr);

	memcpy(buf, dfu->block.readptr, read_len);

	dfu->block.readptr += read_len;

	return read_len;
}

static int stfub_dfu_write_firmware_block(struct stfub_dfu *dfu, u16 block_no,
					  const u8 *buf, int len)
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

static bool stfub_dfu_attribute_is_set(struct stfub_dfu *dfu, u8 attribute)
{
	return dfu->descr->bmAttributes & attribute;
}

static enum dfu_state stfub_dfu_get_state(struct stfub_dfu *dfu)
{
	return dfu->state;
}

static void stfub_dfu_set_state(struct stfub_dfu *dfu,
				 enum dfu_state state)
{
	dfu->state = state;
}

static enum dfu_status stfub_dfu_get_status(struct stfub_dfu *dfu)
{
	return dfu->status;
}

static void stfub_dfu_set_status(struct stfub_dfu *dfu,
				  enum dfu_status status)
{
	dfu->status = status;
}

static u32 stfub_dfu_get_poll_timeout(struct stfub_dfu *dfu)
{
	return dfu->timeout;
}

static bool stfub_dfu_timeout_elapsed(struct stfub_dfu *dfu)
{
	/* FIXME: Implement proper timeout support */
	return true;
}

static int stfub_dfu_handle_get_status_request(struct stfub_dfu *dfu, u8 *buf,
						u16 *len)
{
	u32 timeout = stfub_dfu_get_poll_timeout(dfu);

	buf[0] = stfub_dfu_get_status(dfu);
	buf[1] = timeout & 0xFF;
	buf[2] = (timeout >> 8) & 0xFF;
	buf[3] = (timeout >> 16) & 0xFF;
	buf[4] = stfub_dfu_get_state(dfu);
	buf[5] = 0; /* iString not used here */
	*len   = 6;

	return USBD_REQ_HANDLED;
}

static int stfub_dfu_handle_get_state_request(struct stfub_dfu *dfu, u8 *buf,
					       u16 *len)
{
	buf[0] = stfub_dfu_get_state(dfu);
	*len   = 1;

	return USBD_REQ_HANDLED;
}

static void stfub_dfu_timestamp_poll_request(struct stfub_dfu *dfu)
{
}

static int stfub_dfu_queue_firmware_block(struct stfub_dfu *dfu,
					   u16 block_no, const u8 *buf,
					   int len)
{
	return 0;
}

static bool stfub_dfu_write_pending(struct stfub_dfu *dfu)
{
	return false;
}


static bool dfu_all_data_is_received(struct stfub_dfu *dfu)
{
	return true;
}

void stfub_dfu_tick(void)
{
#if 0
	switch(stfub_dfu_get_state(&dfu)) {
	case STATE_DFU_DNBUSY:
		if (stfub_dfu_write_pending(&dfu))
			stfub_dfu_write_firmware_block(&dfu);
		stfub_dfu_set_state(&dfu, STATE_DFU_DNLOAD_SYNC);
		break;
	case STATE_DFU_MANIFEST:
		if (stfub_dfu_timeout_elapsed(&dfu)) {
			if (stfub_dfu_attribute_is_set(&dfu, USB_DFU_MANIFEST_TOLERANT))
				stfub_dfu_set_state(&dfu, STATE_DFU_MANIFEST_SYNC);
			else
				stfub_dfu_set_state(&dfu, STATE_DFU_MANIFEST_WAIT_RESET);
		}
		break;
	default:
		break;

	}
#endif
}

int stfub_dfu_handle_control_request(usbd_device *udbddev, struct usb_setup_data *req, u8 **buf,
				     u16 *len, void (**complete)(usbd_device *udbddev, struct usb_setup_data *req))
{
	int read_size, requested_size;

	if ((req->bmRequestType & 0x7F) != 0x21)
		return USBD_REQ_NOTSUPP; /* Only accept class request. */

	stfub_dfu_timestamp_poll_request(&dfu);

	switch(stfub_dfu_get_state(&dfu)) {
	/* Table A.2.3 of the official DFU spec v 1.1 */
	case STATE_DFU_IDLE:
		switch (req->bRequest) {
		case DFU_DNLOAD:
			if ((len == NULL) || (*len == 0) ||
			    !stfub_dfu_attribute_is_set(&dfu, USB_DFU_CAN_DOWNLOAD)) {
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}
			if (stfub_dfu_queue_firmware_block(&dfu, req->wValue,
							    *buf, *len) < 0) {
				stfub_dfu_set_status(&dfu, DFU_STATUS_ERR_UNKNOWN);
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}
 			stfub_dfu_set_state(&dfu, STATE_DFU_DNLOAD_SYNC);
			return USBD_REQ_HANDLED;
		case DFU_UPLOAD:
			if (!stfub_dfu_attribute_is_set(&dfu, USB_DFU_CAN_UPLOAD)) {
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}

			read_size = stfub_dfu_read_firmware_block(&dfu, req->wValue,
								  *buf, *len);
			if(read_size < 0) {
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				*len = 0;
				return USBD_REQ_NOTSUPP;
			} else {
				stfub_dfu_set_state(&dfu, STATE_DFU_UPLOAD_IDLE);
				*len = read_size;
				return USBD_REQ_HANDLED;
			}
		case DFU_ABORT:
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		default:
			break;
		}
		break;
	/* Table A.2.4 */
	case STATE_DFU_DNLOAD_SYNC:
		switch (req->bRequest) {
		case DFU_GETSTATUS:
			/* Since we are working in a polling mode the
			 * transfer will always be complete by the
			 * time this request is served */
			stfub_dfu_set_state(&dfu, STATE_DFU_DNLOAD_IDLE);
			return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		default:
			break;
		}
		break;
	/* Table A.2.5 */
	case STATE_DFU_DNBUSY:
		break;
	/* Table A.2.6 */
	case STATE_DFU_DNLOAD_IDLE:
		switch (req->bRequest) {
		case DFU_DNLOAD:
			if ((len == NULL) || (*len == 0)) {
				if (dfu_all_data_is_received(&dfu)) {
					stfub_dfu_set_state(&dfu, STATE_DFU_MANIFEST_SYNC);
					return USBD_REQ_HANDLED;
				} else {
					stfub_dfu_set_status(&dfu, DFU_STATUS_ERR_NOTDONE);
					stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
					return USBD_REQ_NOTSUPP;
				}
			}

			if (stfub_dfu_queue_firmware_block(&dfu, req->wValue,
							    *buf, *len) < 0) {
				stfub_dfu_set_status(&dfu, DFU_STATUS_ERR_UNKNOWN);
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}
 			stfub_dfu_set_state(&dfu, STATE_DFU_DNLOAD_SYNC);
			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			stfub_dfu_set_state(&dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			/* Since we are working in a polling mode the
			 * transfer will always be complete by the
			 * time this request is served */
			stfub_dfu_set_state(&dfu, STATE_DFU_DNLOAD_IDLE);
			return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		default:
			break;
		}
		break;
	/* Table A.2.7 */
	case STATE_DFU_MANIFEST_SYNC:
		switch (req->bRequest) {
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		case DFU_GETSTATUS:
			if (stfub_dfu_attribute_is_set(&dfu, USB_DFU_MANIFEST_TOLERANT)) {
				/* Since we are working in a polling mode the
				 * manifestation will always be complete by the
				 * time this request is served */
				stfub_dfu_set_state(&dfu, STATE_DFU_IDLE);
				return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
			}
		default:	/* FALLTHROUGH */
			break;
		}
		break;
	/* Table A.2.8 */
	case STATE_DFU_MANIFEST:
		/* Should never get here */
		break;
	/* Table A.2.9 */
	case STATE_DFU_MANIFEST_WAIT_RESET:
		return USBD_REQ_HANDLED;

	case STATE_DFU_UPLOAD_IDLE:
		switch (req->bRequest) {
		case DFU_UPLOAD:
			requested_size = *len;
			read_size = stfub_dfu_read_firmware_block(&dfu, req->wValue,
								  *buf, *len);
			*len = MIN(*len, read_size);

			if (read_size < 0) {
				stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
				*len = 0;
			}
			else if (read_size < requested_size) {
				stfub_dfu_set_state(&dfu, STATE_DFU_IDLE);
			}
			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			stfub_dfu_set_state(&dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		default:
			break;
		}
		break;

	case STATE_DFU_ERROR:
		switch (req->bRequest) {
		case DFU_GETSTATUS:
			return stfub_dfu_handle_get_status_request(&dfu, *buf, len);
		case DFU_GETSTATE:
			return stfub_dfu_handle_get_state_request(&dfu, *buf, len);
		case DFU_CLRSTATUS:
			stfub_dfu_set_status(&dfu, DFU_STATUS_OK);
			stfub_dfu_set_state(&dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		default:
			break;
		}
		break;
	default:
		break;
	}

	stfub_dfu_set_status(&dfu, DFU_STATUS_ERR_STALLEDPKT);
	stfub_dfu_set_state(&dfu, STATE_DFU_ERROR);
	return USBD_REQ_NOTSUPP;
}
