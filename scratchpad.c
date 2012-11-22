#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/crc.h>
/* 
   Total size of scratch area is 32 bytes
 */


struct stfub_scratchpad {
	uint8_t  boot_to_dfu;
	uint8_t  __reserved[27];
	uint32_t crc;
} __attribute__ ((packed));

extern unsigned _scratch;

static void stfub_scratchpad_recalculate_crc(void)
{
	struct stfub_scratchpad *scratchpad;

	scratchpad = (struct stfub_scratchpad *)&_scratch;
	scratchpad->crc = 0;
	scratchpad->crc = crc_calculate_block((uint32_t *)scratchpad,
					      sizeof(*scratchpad) / 4);
}

bool stfub_scratchpad_is_valid(void)
{
	u32 crc;
	struct stfub_scratchpad *scratchpad;

	scratchpad = (struct stfub_scratchpad *)&_scratch;

	crc = crc_calculate_block((uint32_t *)scratchpad,
				  sizeof(*scratchpad) / 4);

	return crc == scratchpad->crc;
}

bool stfub_scratchpad_dfu_switch_requested(void)
{
	struct stfub_scratchpad *scratchpad;

	scratchpad = (struct stfub_scratchpad *)&_scratch;

	return scratchpad->boot_to_dfu;
}

void stfub_scratchpad_request_dfu_switch(void)
{
	struct stfub_scratchpad *scratchpad;

	scratchpad = (struct stfub_scratchpad *)&_scratch;

	scratchpad->boot_to_dfu = true;
	stfub_scratchpad_recalculate_crc();
}

void stfub_scratchpad_init(void)
{
	struct stfub_scratchpad *scratchpad;
	scratchpad = (struct stfub_scratchpad *)&_scratch;

	memset(scratchpad, 0, sizeof(*scratchpad));
}
