#ifndef __LIBSTFUB_INFO_BLOCK_H__
#define __LIBSTFUB_INFO_BLOCK_H__

#include <stdint.h>

/* Total size is 512 */
struct stfub_firmware_info {
	uint32_t size;
	uint8_t  __reserved[500];
	struct {
		uint32_t firmware;
		uint32_t info_block;
	} crc;
} __attribute__((packed));


#endif	/* __LIBSTFUB_INFO_BLOCK_H__ */
