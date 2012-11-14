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

static bool dfu_device_attribute_is_set(struct dfu_device *dfu, u8 attribute)
{
	return dfu->descr->bmAttributes & attribute;
}

static enum dfu_state dfu_device_get_state(struct dfu_device *dfu)
{
	return dfu->state;
}

static void dfu_device_set_state(struct dfu_device *dfu,
				 enum dfu_state state)
{
	dfu->state = state;
}

static enum dfu_status dfu_device_get_status(struct dfu_device *dfu)
{
	return dfu->status;
}

static void dfu_device_set_status(struct dfu_device *dfu,
				  enum dfu_status status)
{
	dfu->status = status;
}

static u32 dfu_device_get_pool_timeout(struct dfu_device *dfu)
{
	return dfu->timeout;
}

static bool dfu_device_timeout_elapsed(struct dfu_device *dfu)
{
	/* FIXME: Implement proper timeout support */
	return true;
}

static int dfu_device_handle_get_status_request(struct dfu_device *dfu, u8 *buf,
						u16 *len)
{
	u32 timeout = dfu_device_get_pool_timeout(dfu);

	buf[0] = dfu_device_get_status(dfu);
	buf[1] = timeout & 0xFF;
	buf[2] = (timeout >> 8) & 0xFF;
	buf[3] = (timeout >> 16) & 0xFF;
	buf[4] = dfu_device_get_state(dfu);
	buf[5] = 0; /* iString not used here */
	*len   = 6;

	return USBD_REQ_HANDLED;
}

static int dfu_device_handle_get_state_request(struct dfu_device *dfu, u8 *buf,
					       u16 *len)
{
	buf[0] = dfu_device_get_state(dfu);
	*len   = 1;

	return USBD_REQ_HANDLED;
}

static void dfu_device_timestamp_poll_request(struct dfu_device *dfu)
{
}

static int dfu_device_read_firmware_block(struct dfu_device *dfu,
					  u16 block_no, u8 *buf,
					  int len)
{
	return dfu->ops->read_block(dfu, block_no, buf, len, dfu->context);
}

static void dfu_device_write_firmware_block(struct dfu_device *dfu)
{
	dfu->ops->write_block(dfu, dfu->block.num, dfu->block.buf,
			      dfu->block.len, dfu->context);
	dfu->block.len = 0;
}

static int dfu_device_queue_firmware_block(struct dfu_device *dfu,
					   u16 block_no, const u8 *buf,
					   int len)
{
	/* Copy download data for use on GET_STATUS. */
	if (len > dfu->block.buf_size)
		return -1;

	memcpy(dfu->block.buf, buf, len);
	dfu->block.len = len;

	return 0;
}

static bool dfu_device_write_pending(struct dfu_device *dfu)
{
	return dfu->block.len > 0;
}


static bool dfu_all_data_is_received(struct dfu_device *dfu)
{
	return dfu->ops->all_data_in(dfu, dfu->context);
}

void dfu_device_init(struct dfu_device *dfu,
		     const struct usb_dfu_descriptor *descr,
		     const struct dfu_device_ops     *ops,
		     u8 *buf, int buf_size, void *context)
{
	dfu->state	= STATE_DFU_IDLE;
	dfu->status	= DFU_STATUS_OK;

	dfu->descr = descr;
	dfu->ops  = ops;

	dfu->block.buf		= buf;
	dfu->block.buf_size	= buf_size;

	dfu->timeout = 100;
	dfu->context = context;
}


void dfu_device_tick(struct dfu_device *dfu)
{
	switch(dfu->state) {
	case STATE_DFU_DNBUSY:
		if (dfu_device_write_pending(dfu))
			dfu_device_write_firmware_block(dfu);
		dfu_device_set_state(dfu, STATE_DFU_DNLOAD_SYNC);
		break;
	case STATE_DFU_MANIFEST:
		if (dfu_device_timeout_elapsed(dfu)) {
			if (dfu_device_attribute_is_set(dfu, USB_DFU_MANIFEST_TOLERANT))
				dfu_device_set_state(dfu, STATE_DFU_MANIFEST_SYNC);
			else
				dfu_device_set_state(dfu, STATE_DFU_MANIFEST_WAIT_RESET);
		}
		break;
	default:
		break;

	}
}


/* FIXME: This function should not even exist and the only reason it
 * exist is because libopencm3 api for control callback does not allow
 * for passing of a void * to tsomme sort of a context. This needs to
 * be fixed(alongside with limitation for the number of callbacks)
 */
struct dfu_device *dfu_device_get_dfu_device(void)
{
	static struct dfu_device dfu;
	return &dfu;
}


int dfu_device_handle_control_request(usbd_device *udbddev, struct usb_setup_data *req, u8 **buf,
				      u16 *len, void (**complete)(usbd_device *udbddev, struct usb_setup_data *req))
{
	struct dfu_device *dfu;
	int read_size, requested_size;

	if ((req->bmRequestType & 0x7F) != 0x21)
		return USBD_REQ_NOTSUPP; /* Only accept class request. */

	dfu = dfu_device_get_dfu_device();

	dfu_device_timestamp_poll_request(dfu);

	switch(dfu_device_get_state(dfu)) {
	/* Table A.2.3 of the official DFU spec v 1.1 */
	case STATE_DFU_IDLE:
		switch (req->bRequest) {
		case DFU_DNLOAD:
			if ((len == NULL) || (*len == 0) ||
			    !dfu_device_attribute_is_set(dfu, USB_DFU_CAN_DOWNLOAD)) {
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}

			if (dfu_device_queue_firmware_block(dfu, req->wValue,
							    *buf, *len) < 0) {
				dfu_device_set_status(dfu, DFU_STATUS_ERR_UNKNOWN);
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}
 			dfu_device_set_state(dfu, STATE_DFU_DNLOAD_SYNC);
			return USBD_REQ_HANDLED;
		case DFU_UPLOAD:
			if (!dfu_device_attribute_is_set(dfu, USB_DFU_CAN_UPLOAD)) {
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}

			dfu_device_set_state(dfu, STATE_DFU_UPLOAD_IDLE);
			if(dfu_device_read_firmware_block(dfu, req->wValue,
							  *buf, *len) < 0) {
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				*len = 0;
			}
			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			return dfu_device_handle_get_status_request(dfu, *buf, len);
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
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
			dfu_device_set_state(dfu, STATE_DFU_DNLOAD_IDLE);
			return dfu_device_handle_get_status_request(dfu, *buf, len);
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
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
				if (dfu_all_data_is_received(dfu)) {
					dfu_device_set_state(dfu, STATE_DFU_MANIFEST_SYNC);
					return USBD_REQ_HANDLED;
				} else {
					dfu_device_set_status(dfu, DFU_STATUS_ERR_NOTDONE);
					dfu_device_set_state(dfu, STATE_DFU_ERROR);
					return USBD_REQ_NOTSUPP;
				}
			}

			if (dfu_device_queue_firmware_block(dfu, req->wValue,
							    *buf, *len) < 0) {
				dfu_device_set_status(dfu, DFU_STATUS_ERR_UNKNOWN);
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				return USBD_REQ_NOTSUPP;
			}
 			dfu_device_set_state(dfu, STATE_DFU_DNLOAD_SYNC);
			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			dfu_device_set_state(dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			/* Since we are working in a polling mode the
			 * transfer will always be complete by the
			 * time this request is served */
			dfu_device_set_state(dfu, STATE_DFU_DNLOAD_IDLE);
			return dfu_device_handle_get_status_request(dfu, *buf, len);
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
		default:
			break;
		}
		break;
	/* Table A.2.7 */
	case STATE_DFU_MANIFEST_SYNC:
		switch (req->bRequest) {
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
		case DFU_GETSTATUS:
			if (dfu_device_attribute_is_set(dfu, USB_DFU_MANIFEST_TOLERANT)) {
				/* Since we are working in a polling mode the
				 * manifestation will always be complete by the
				 * time this request is served */
				dfu_device_set_state(dfu, STATE_DFU_IDLE);
				return dfu_device_handle_get_status_request(dfu, *buf, len);
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
			read_size = dfu_device_read_firmware_block(dfu, req->wValue,
								   *buf, *len);
			*len = MIN(*len, read_size);

			if (read_size < 0) {
				dfu_device_set_state(dfu, STATE_DFU_ERROR);
				*len = 0;
			}
			else if (read_size < requested_size) {
				dfu_device_set_state(dfu, STATE_DFU_IDLE);
			}

			return USBD_REQ_HANDLED;
		case DFU_ABORT:
			dfu_device_set_state(dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		case DFU_GETSTATUS:
			return dfu_device_handle_get_status_request(dfu, *buf, len);
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
		default:
			break;
		}
		break;

	case STATE_DFU_ERROR:
		switch (req->bRequest) {
		case DFU_GETSTATUS:
			return dfu_device_handle_get_status_request(dfu, *buf, len);
		case DFU_GETSTATE:
			return dfu_device_handle_get_state_request(dfu, *buf, len);
		case DFU_CLRSTATUS:
			dfu_device_set_status(dfu, DFU_STATUS_OK);
			dfu_device_set_state(dfu, STATE_DFU_IDLE);
			return USBD_REQ_HANDLED;
		default:
			break;
		}
		break;
	default:
		break;
	}

	dfu_device_set_status(dfu, DFU_STATUS_ERR_STALLEDPKT);
	dfu_device_set_state(dfu, STATE_DFU_ERROR);
	return USBD_REQ_NOTSUPP;
}
