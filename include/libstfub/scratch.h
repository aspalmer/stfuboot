#ifndef __LIBSTFUB_SCRATCH_H__
#define __LIBSTFUB_SCRATCH_H__

bool stfub_scratchpad_is_valid(void);
bool stfub_scratchpad_dfu_switch_requested(void);
void stfub_scratchpad_request_dfu_switch(void);
void stfub_scratchpad_init(void);

#endif	/* __LIBSTFUB_SCRATCH_H__ */
